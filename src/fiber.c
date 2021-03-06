#include "fiber.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ucontext.h>
#include <ucontext.h>
#include <unistd.h>

#define REG_RSP 15
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
        struct ThreadCarrier *tc = calloc(sizeof(struct ThreadCarrier), 1);
        tc->running_queue.size = qsize;
        tc->running_queue.queue = calloc(qsize, sizeof(fiber_t));
        tc->shared_stack = calloc(1, FIBER_STACK_SIZE);
        tc->ctx.uc_stack.ss_sp = calloc(1, FIBER_STACK_SIZE);
        sem_init(&tc->running_queue.sem_used, 0, 0);
        sem_init(&tc->running_queue.sem_free, 0, qsize);
        sch->threads[i] = tc;
        tc->sch = sch;
    }

    return sch;
}

void free_scheduler(struct Scheduler *sch) {
    int i = 0;
    for (; i < sch->thread_size; ++i) {
        struct ThreadCarrier *tc = sch->threads[i];
        sem_destroy(&tc->running_queue.sem_free);
        sem_destroy(&tc->running_queue.sem_used);
        free(tc->shared_stack);
        free(tc->ctx.uc_stack.ss_sp);
        free(tc->running_queue.queue);
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
    sch->stop = 1;

    int i = 0;
    for (; i < sch->thread_size; ++i) {
        sch->threads[i]->stop = 1;
    }

    for (i = 0; i < sch->thread_size; ++i) {
        int sval;
        struct ThreadCarrier *tc = sch->threads[i];
        sem_getvalue(&tc->running_queue.sem_free, &sval);
        if (sval != tc->running_queue.size) {
            sem_wait(&tc->done);
        }
    }

    void *status;
    for (i = 0; i < sch->thread_size; ++i) {
        pthread_cancel(sch->threads[i]->tid);
        pthread_join(sch->threads[i]->tid, &status);
    }
}

void start_io_dispatcher(struct Scheduler *sch) {
    assert(pthread_create(&sch->dispatcher_tid, NULL,
                          io_dispatch_thread, (void*)sch) == 0);
}

void stop_io_dispatcher(struct Scheduler *sch) {
    pthread_cancel(sch->dispatcher_tid);
    void *status;
    pthread_join(sch->dispatcher_tid, &status);
}

void switch_to_scheduler(fiber_t fiber) {
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
}

void switch_to_fiber(fiber_t fiber) {
    if (fiber->ctx.uc_stack.ss_sp != fiber->tc->ctx.uc_stack.ss_sp) {
        char *stack_end = fiber->tc->shared_stack + FIBER_STACK_SIZE - fiber->ctx.uc_stack.ss_size;
        memcpy(stack_end, fiber->ctx.uc_stack.ss_sp, fiber->ctx.uc_stack.ss_size);
        free(fiber->ctx.uc_stack.ss_sp);
        fiber->ctx.uc_stack.ss_sp = fiber->tc->shared_stack;
        fiber->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
    }

    swapcontext(&fiber->tc->ctx, &fiber->ctx);

    int stack_size = (uint64_t)fiber->ctx.uc_stack.ss_sp + FIBER_STACK_SIZE
        - fiber->ctx.uc_mcontext.gregs[REG_RSP];
    char *fiber_stack = calloc(1, stack_size);
    memcpy(fiber_stack, (char*)fiber->ctx.uc_mcontext.gregs[REG_RSP], stack_size);
    fiber->ctx.uc_stack.ss_sp = fiber_stack;
    fiber->ctx.uc_stack.ss_size = stack_size;
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
    if (sch == NULL || fiber == NULL || sch->stop) {
        return -1;
    }

    struct ThreadCarrier *tc = sch->threads[sch->index];
    while (sem_trywait(&tc->running_queue.sem_free) != 0) {
        sch->index = (++sch->index) & (sch->thread_size - 1);
        tc = sch->threads[sch->index];
    }

    fiber->tc = tc;
    int index = (int)(__sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1));
    tc->running_queue.queue[index] = fiber;

    sem_post(&tc->running_queue.sem_used);
    sch->index = (++sch->index) & (sch->thread_size - 1);
    return 0;
}

int yield(fiber_t fiber) {
    if (fiber == NULL) {
        return -1;
    }

    fiber->status = RUNABLE;
    struct ThreadCarrier *tc = fiber->tc;
    tc->running_queue.queue[tc->running_queue.tail] = NULL;
    int index = (int)(__sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1));
    tc->running_queue.queue[index] = fiber;
    sem_post(&tc->running_queue.sem_used);
    switch_to_scheduler(fiber);
    return 0;
}

int detach_fiber(fiber_t fiber) {
    if (fiber == NULL) {
        return -1;
    }

    struct ThreadCarrier *tc = fiber->tc;
    tc->running_queue.queue[tc->running_queue.tail] = NULL;
    sem_post(&tc->running_queue.sem_free);
    return 0;
}

int suspend_fiber(fiber_t fiber, int fd) {
    if (fiber == NULL || fd < 0) {
        return -1;
    }

    fiber->status = SUSPEND;
    struct ThreadCarrier *tc = fiber->tc;
    tc->sch->blocked_io_set[fd] = fiber;
    tc->running_queue.queue[tc->running_queue.tail] = NULL;
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
    int index = (int)(__sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1));
    tc->running_queue.queue[index] = fiber;
    sem_post(&tc->running_queue.sem_used);
    return 0;
}

void fiber_entry(fiber_t fiber) {
    fiber->user_func(fiber, fiber->user_data);
    fiber->status = DEAD;
    detach_fiber(fiber);
    if (fiber->tc->stop) {
        int sval;
        struct ThreadCarrier *tc = fiber->tc;
        sem_getvalue(&tc->running_queue.sem_free, &sval);
        if (sval == tc->running_queue.size) {
            sem_post(&tc->done);
        }
    }
    switch_to_scheduler(fiber);
}

void* schedule_thread(void *data) {
    struct ThreadCarrier *tc = (struct ThreadCarrier*)data;
    fiber_t fiber;

    while(1) {
        sem_wait(&tc->running_queue.sem_used);

        while (tc->running_queue.queue[tc->running_queue.tail] == NULL) {
            usleep(1);
        }
        while (tc->running_queue.queue[tc->running_queue.tail]) {
            fiber = tc->running_queue.queue[tc->running_queue.tail];
            switch (fiber->status) {
                case READY:
                    getcontext(&fiber->ctx);
                    fiber->ctx.uc_stack.ss_sp = tc->shared_stack;
                    fiber->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
                    makecontext(&fiber->ctx, (void(*)(void))fiber->entry, 1, fiber);

                case RUNABLE:
                    fiber->status = RUNNING;
                    tc->running_fiber = fiber;
                    switch_to_fiber(fiber);
                    break;

                default:
                    break;
            }
            if (fiber->status == DEAD) {
                free(fiber->ctx.uc_stack.ss_sp);
                free(fiber);
            }
        }
        tc->running_queue.tail = (++tc->running_queue.tail) & (tc->running_queue.size - 1);
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
