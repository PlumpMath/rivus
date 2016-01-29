#include "fiber.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <ucontext.h>

#define RIVUS_NR_OPEN   1024*64


int suspend_fiber(fiber_t fiber, int fd);
int wake_fiber(struct Scheduler *sch, int fd);

static void fiber_entry(fiber_t fiber);
static int detach_fiber(fiber_t fiber);

static void* schedule_thread(void *data);
static void* io_dispatch_thread(void *data);


struct Scheduler* create_scheduler(int thread_nums) {
    if (thread_nums <= 0 || thread_nums > 256) {
        return NULL;
    }

    int tnums = 1;
    while (thread_nums > tnums) {
        tnums = tnums * 2;
    }
    int qsize = 1024 * 64 / tnums;

    struct Scheduler *sch = calloc(1, sizeof(struct Scheduler));
    sch->thread_size = tnums;
    sch->threads = calloc(tnums, sizeof(struct ThreadCarrier*));
    sch->blocked_io_set = calloc(RIVUS_NR_OPEN, sizeof(fiber_t));

    int i = 0;
    for (; i < tnums; ++i) {
        struct ThreadCarrier *tc = calloc(1, sizeof(struct ThreadCarrier));
        tc->fiber_queue.size = qsize;
        tc->fiber_queue.head = 0;
        tc->fiber_queue.tail = 0;
        tc->fiber_queue.queue = calloc(qsize, sizeof(fiber_t));
        sem_init(&tc->fiber_queue.sem_used, 0, 0);
        sem_init(&tc->fiber_queue.sem_free, 0, qsize);
        sch->threads[i] = tc;
        tc->sch = sch;
    }

    return sch;
}

void free_scheduler(struct Scheduler *sch) {
    int i = 0;
    for (; i < sch->thread_size; ++i) {
        struct ThreadCarrier *tc = sch->threads[i];
        sem_destroy(&tc->fiber_queue.sem_free);
        sem_destroy(&tc->fiber_queue.sem_used);
        free(tc->fiber_queue.queue);
        free(tc);
    }

    free(sch->threads);
    free(sch->blocked_io_set);
    free(sch);
}

void start_scheduler(struct Scheduler *sch) {
    int i = 0;
    for (; i < sch->thread_size; ++i) {
        assert(pthread_create(&sch->threads[i]->tid,
                       NULL, schedule_thread, (void*)sch->threads[i]) == 0);
    }
}

void stop_scheduler(struct Scheduler *sch) {
    int i = 0;
    void *status;
    for (; i < sch->thread_size; ++i) {
        pthread_join(sch->threads[i]->tid, &status);
    }
}

void start_io_dispatcher(struct Scheduler *sch) {
    assert(pthread_create(&sch->dispatcher_tid, NULL,
                          io_dispatch_thread, (void*)sch) == 0);
}

void stop_io_dispatcher(struct Scheduler *sch) {
    void *status;
    pthread_join(sch->dispatcher_tid, &status);
}

int create_fiber(fiber_t *fiber, void (*user_func)(fiber_t, void*), void *data) {
    if (fiber == NULL) {
        return -1;
    }

    *fiber = calloc(1, sizeof(struct Fiber));
    (*fiber)->entry = fiber_entry;
    (*fiber)->user_func = user_func;
    (*fiber)->user_data = data;
    (*fiber)->status = READY;
    return 0;
}

int schedule(struct Scheduler *sch, fiber_t fiber) {
    if (sch == NULL || fiber == NULL) {
        return -1;
    }

    struct ThreadCarrier *tc = sch->threads[sch->index];
    while (sem_trywait(&tc->fiber_queue.sem_free) != 0) {
        sch->index = (++sch->index) & (sch->thread_size - 1);
        tc = sch->threads[sch->index];
    }

    fiber->tc = tc;
    int index = (int)(__sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1));
    tc->fiber_queue.queue[index] = fiber;

    sem_post(&tc->fiber_queue.sem_used);
    sch->index = (++sch->index) & (sch->thread_size - 1);

    return 0;
}

int yield(fiber_t fiber) {
    if (fiber == NULL) {
        return -1;
    }
    fiber->status = RUNABLE;
    struct ThreadCarrier *tc = fiber->tc;
    tc->fiber_queue.queue[tc->fiber_queue.tail] = NULL;
    int index = (int)(__sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1));
    tc->fiber_queue.queue[index] = fiber;
    sem_post(&tc->fiber_queue.sem_used);
    swapcontext(&fiber->ctx, &tc->ctx);
    return 0;
}

int detach_fiber(fiber_t fiber) {
    if (fiber == NULL) {
        return -1;
    }

    struct ThreadCarrier *tc = fiber->tc;
    tc->fiber_queue.queue[tc->fiber_queue.tail] = NULL;
    sem_post(&tc->fiber_queue.sem_free);
    return 0;
}

int suspend_fiber(fiber_t fiber, int fd) {
    if (fiber == NULL || fd < 0) {
        return -1;
    }

    fiber->status = SUSPEND;
    struct ThreadCarrier *tc = fiber->tc;
    tc->sch->blocked_io_set[fd] = fiber;
    tc->fiber_queue.queue[tc->fiber_queue.tail] = NULL;
    return 0;
}

int wake_fiber(struct Scheduler* sch, int fd) {
    if (sch == NULL || fd < 0) {
        return -1;
    }
    fiber_t fiber = sch->blocked_io_set[fd];
    if (fiber == NULL) {
        return -1;
    }

    fiber->status = RUNABLE;
    struct ThreadCarrier *tc = fiber->tc;
    int index = (int)(__sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1));
    tc->fiber_queue.queue[index] = fiber;
    sem_post(&tc->fiber_queue.sem_used);
    return 0;
}

void fiber_entry(fiber_t fiber) {
    fiber->user_func(fiber, fiber->user_data);
    fiber->status = DEAD;
    detach_fiber(fiber);
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
}

void* schedule_thread(void *data) {
    struct ThreadCarrier *tc = (struct ThreadCarrier*)data;
    fiber_t fiber;
    while (1) {
        sem_wait(&tc->fiber_queue.sem_used);
        while (tc->fiber_queue.queue[tc->fiber_queue.tail] == NULL) {
            usleep(1);
        }
        while (tc->fiber_queue.queue[tc->fiber_queue.tail]) {
            fiber = tc->fiber_queue.queue[tc->fiber_queue.tail];
            switch (fiber->status) {
                case READY:
                    getcontext(&fiber->ctx);
                    fiber->ctx.uc_stack.ss_sp = fiber->stack;
                    fiber->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
                    makecontext(&fiber->ctx,
                                (void(*)(void))fiber->entry, 1, fiber);
                case RUNABLE:
                    fiber->status = RUNNING;
                    tc->running_fiber = fiber;
                    swapcontext(&tc->ctx, &fiber->ctx);
                    break;
                default:
                    break;
            }
            if (fiber->status == DEAD) {
                free(fiber);
            }
        }
        tc->fiber_queue.tail = (++tc->fiber_queue.tail) & (tc->fiber_queue.size - 1);
    }
}

void* io_dispatch_thread(void *data) {
    struct Scheduler *sch = (struct Scheduler*)data;
    struct epoll_event ev;
    struct epoll_event *events;
    int nfds = 0;

    sch->epoll_fd = epoll_create(32);
    assert(sch->epoll_fd >= 0);

    events = calloc(MAX_EVENT_SIZE, sizeof(struct epoll_event));
    while (1) {
        nfds = epoll_wait(sch->epoll_fd, events, MAX_EVENT_SIZE, -1);

        int i = 0;
        for (; i < nfds; ++i) {
            epoll_ctl(sch->epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
            wake_fiber(sch, events[i].data.fd);
        }
    }
    free(events);
    close(sch->epoll_fd);
}
