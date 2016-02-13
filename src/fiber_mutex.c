#include "fiber_mutex.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <ucontext.h>

#include "fiber.h"


#define MUTEX_WAIT_QUEUE_SIZE   1024*64
#define MUTEX_WAIT_QUEUE_INDEX_MASK     0xffff
#define MUTEX_WAIT_QUEUE_LEN_MASK       0xffffffff


int fiber_mutex_init(fiber_mutex_t *f_mtx) {
    if (f_mtx == NULL) {
        return -1;
    }

    (*f_mtx) = malloc(sizeof(struct FiberMutex));
    (*f_mtx)->value = 0x0;
    (*f_mtx)->owner = NULL;
    (*f_mtx)->wait_queue = calloc(MUTEX_WAIT_QUEUE_SIZE, sizeof(struct Fiber*));
    return 0;
}

int fiber_mutex_destroy(fiber_mutex_t *f_mtx) {
    if (!(*f_mtx) || ((*f_mtx)->value & MUTEX_WAIT_QUEUE_LEN_MASK) != 0) {
        return -1;
    }

    free((*f_mtx)->wait_queue);
    free(*f_mtx);
    return 0;
}

int fiber_mutex_lock(fiber_t fiber, fiber_mutex_t *f_mtx) {
    /* nested lock */
    assert(fiber != (*f_mtx)->owner);

    uint64_t value = __sync_fetch_and_add(&(*f_mtx)->value, 0x100000001);
    uint32_t len = value & MUTEX_WAIT_QUEUE_LEN_MASK;

    assert(len < MUTEX_WAIT_QUEUE_SIZE);

    if (len == 0) {
        /* no fiber is holding the lock, got the lock */
        (*f_mtx)->owner = fiber;
        return 0;
    }

    /* suspend the fiber, and put it on the wait queue */
    fiber->status = SUSPEND;
    uint32_t index = (value >> 32) & MUTEX_WAIT_QUEUE_INDEX_MASK;
    (*f_mtx)->wait_queue[index] = fiber;

    fiber->tc->running_queue.queue[fiber->tc->running_queue.tail] = NULL;
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
    return 0;
}

int fiber_mutex_unlock(fiber_t fiber, fiber_mutex_t *f_mtx) {
    assert(fiber == (*f_mtx)->owner);

    (*f_mtx)->owner = NULL;
    uint64_t value = __sync_sub_and_fetch(&(*f_mtx)->value, 0x1);
    uint32_t len = value & MUTEX_WAIT_QUEUE_LEN_MASK;
    if (len == 0) {
        /* no fiber is on the wait queue, return immediately */
        return 0;
    }

    /* get the wait queue tail index, the fiber must be woke up was put on there */
    uint32_t index = ((value >> 32) - len) & MUTEX_WAIT_QUEUE_INDEX_MASK;
    while ((*f_mtx)->wait_queue[index] == NULL) {
        usleep(1);
    }

    /* remove the fiber from the wait queue */
    fiber_t waked_fiber = (*f_mtx)->wait_queue[index];
    (*f_mtx)->wait_queue[index] = NULL;
    waked_fiber->status = RUNABLE;
    (*f_mtx)->owner = waked_fiber;

    /* add the fiber to the scheduler running queue */
    struct ThreadCarrier *tc = waked_fiber->tc;
    uint16_t pos = __sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1);
    tc->running_queue.queue[pos] = waked_fiber;
    sem_post(&tc->running_queue.sem_used);
    return 0;
}
