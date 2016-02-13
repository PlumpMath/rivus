#include "fiber_rwlock.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <ucontext.h>

#include "fiber.h"


#define RWLOCK_WAIT_QUEUE_SIZE          1024 * 64
#define RWLOCK_WAIT_QUEUE_INDEX_MASK    0xffff
#define RWLOCK_WAIT_QUEUE_LEN_MASK      0xfffff


static int wr_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);
static int rd_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);


int fiber_rwlock_init(fiber_rwlock_t *f_rwlock) {
    if (f_rwlock == NULL) {
        return -1;
    }

    (*f_rwlock) = malloc(sizeof(struct FiberRWLock));
    (*f_rwlock)->value = 0x0;
    (*f_rwlock)->wr_owner = NULL;
    (*f_rwlock)->status = NO_LOCK;
    (*f_rwlock)->first_wr_index = -1;
    (*f_rwlock)->rd_count = 0;
    (*f_rwlock)->wait_queue = calloc(RWLOCK_WAIT_QUEUE_SIZE, sizeof(struct RWLockedFiber));
    return 0;
}

int fiber_rwlock_destroy(fiber_rwlock_t *f_rwlock) {
    if (!(*f_rwlock) ||
        ((*f_rwlock)->value & RWLOCK_WAIT_QUEUE_LEN_MASK & (RWLOCK_WAIT_QUEUE_LEN_MASK << 20)) != 0x0) {
        return -1;
    }

    free((*f_rwlock)->wait_queue);
    free(*f_rwlock);
    return 0;
}

int fiber_rwlock_rdlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    uint64_t value = __sync_fetch_and_add(&(*f_rwlock)->value, 0x10000000001);
    uint32_t rd_len = (value) & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint32_t wr_len = (value >> 20) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert((rd_len + wr_len) < RWLOCK_WAIT_QUEUE_SIZE);

    uint32_t index = (value >> 40) & RWLOCK_WAIT_QUEUE_INDEX_MASK;
    if (wr_len == 0) {
        /* no writer is holding or waiting for the lock, got the lock */
        (*f_rwlock)->status = RD_LOCK;
        return 0;
    }

    /* suspend the fiber and put it on the wait queue */
    fiber->status = SUSPEND;
    (*f_rwlock)->wait_queue[index].status = RD_WAIT;
    (*f_rwlock)->wait_queue[index].fiber = fiber;

    fiber->tc->running_queue.queue[fiber->tc->running_queue.tail] = NULL;
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
    return 0;
}

int fiber_rwlock_wrlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    /* nested lock */
    assert(fiber != (*f_rwlock)->wr_owner);

    uint64_t value = __sync_fetch_and_add(&(*f_rwlock)->value, 0x10000100000);
    uint32_t rd_len = value & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint32_t wr_len = (value >> 20) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert((rd_len + wr_len) < RWLOCK_WAIT_QUEUE_SIZE);

    if (rd_len == 0 && wr_len == 0) {
        /* no fiber is holding the lock, got the lock */
        (*f_rwlock)->status = WR_LOCK;
        (*f_rwlock)->wr_owner = fiber;
        return 0;
    }
    if (wr_len == 0) {
        __sync_add_and_fetch(&(*f_rwlock)->rd_count, rd_len);
    }

    /* suspend the fiber and put it on the wait queue */
    fiber->status = SUSPEND;
    uint32_t index = (value >> 40) & RWLOCK_WAIT_QUEUE_INDEX_MASK;
    (*f_rwlock)->wait_queue[index].status = WR_WAIT;
    (*f_rwlock)->wait_queue[index].fiber = fiber;

    if (wr_len == 0) {
        (*f_rwlock)->first_wr_index = index;
    }

    fiber->tc->running_queue.queue[fiber->tc->running_queue.tail] = NULL;
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
    return 0;
}

int fiber_rwlock_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    assert((*f_rwlock)->status != NO_LOCK);
    if ((*f_rwlock)->status == WR_LOCK) {
        return wr_unlock(fiber, f_rwlock);
    } else {
        return rd_unlock(fiber, f_rwlock);
    }
}

int wr_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    assert(fiber == (*f_rwlock)->wr_owner);

    (*f_rwlock)->wr_owner = NULL;
    (*f_rwlock)->status = NO_LOCK;
    (*f_rwlock)->first_wr_index = -1;
    (*f_rwlock)->rd_count = 0;

    uint64_t value = __sync_sub_and_fetch(&(*f_rwlock)->value, 0x100000);
    uint32_t rd_len = value & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint32_t wr_len = (value >> 20) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert((rd_len + wr_len) < RWLOCK_WAIT_QUEUE_SIZE);

    if (wr_len == 0 && rd_len == 0) {
        /* no fiber is on the wait queue, return immediately */
        return 0;
    }

    /* get the wait queue tail index, the first fiber must be woke up was put on there */
    uint32_t index = ((value >> 40) - wr_len - rd_len) & RWLOCK_WAIT_QUEUE_INDEX_MASK;
    while ((*f_rwlock)->wait_queue[index].fiber == NULL) {
        usleep(1);
    }
    if ((*f_rwlock)->wait_queue[index].status == WR_WAIT) {
        /* wake one writer up */

        /* remove the fiber from the wait queue */
        fiber_t waked_fiber = (*f_rwlock)->wait_queue[index].fiber;
        (*f_rwlock)->wait_queue[index].status = NO_WAIT;
        (*f_rwlock)->wait_queue[index].fiber = NULL;
        waked_fiber->status = RUNABLE;
        (*f_rwlock)->status = WR_LOCK;
        (*f_rwlock)->wr_owner = waked_fiber;

        /* add the fiber to the scheduler running queue */
        struct ThreadCarrier *tc = waked_fiber->tc;
        uint16_t pos = __sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1);
        tc->running_queue.queue[pos] = waked_fiber;
        sem_post(&tc->running_queue.sem_used);
    } else {
        /* wake readers up */

        (*f_rwlock)->status = RD_LOCK;

        /* count how many readers must be woke up*/
        int i = 0;
        for (; i < rd_len + wr_len; ++i) {
            int t = (index + i) & RWLOCK_WAIT_QUEUE_INDEX_MASK;
            while ((*f_rwlock)->wait_queue[t].fiber == NULL) {
                usleep(1);
            }
            if ((*f_rwlock)->wait_queue[t].status == WR_WAIT) {
                (*f_rwlock)->first_wr_index = t;
                (*f_rwlock)->rd_count = i;
                break;
            }
        }

        int len = i;
        for (i = 0; i < len; ++i) {
            int t = (index + i) & RWLOCK_WAIT_QUEUE_INDEX_MASK;

            /* remove the fiber from wait queue */
            fiber_t waked_fiber = (*f_rwlock)->wait_queue[t].fiber;
            (*f_rwlock)->wait_queue[t].status = NO_WAIT;
            (*f_rwlock)->wait_queue[t].fiber = NULL;
            waked_fiber->status = RUNABLE;

            /* add the fiber to the scheduler running queue */
            struct ThreadCarrier *tc = waked_fiber->tc;
            uint16_t pos =
                __sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1);
            tc->running_queue.queue[pos] = waked_fiber;
            sem_post(&tc->running_queue.sem_used);
        }
    }
    return 0;
}

int rd_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    uint64_t value = __sync_sub_and_fetch(&(*f_rwlock)->value, 0x1);
    uint32_t rd_len = value & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint32_t wr_len = (value >> 20) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert((rd_len + wr_len) < RWLOCK_WAIT_QUEUE_SIZE);

    if (wr_len == 0) {
        /* no fiber is on the wait queue, return immediately */
        return 0;
    }

    int rd_count = __sync_sub_and_fetch(&(*f_rwlock)->rd_count, 1);
    if (rd_count != 0) {
        /* not all the readers has released the lock, return */
        return 0;
    }

    /* wake the writer on the wait queue tail up */

    while ((*f_rwlock)->first_wr_index < 0) {
        usleep(1);
    }
    uint32_t index = (*f_rwlock)->first_wr_index;
    while ((*f_rwlock)->wait_queue[index].fiber == NULL) {
        usleep(1);
    }
    /* remove the fiber from the wait queue */
    fiber_t waked_fiber = (*f_rwlock)->wait_queue[index].fiber;
    (*f_rwlock)->wait_queue[index].status = NO_WAIT;
    (*f_rwlock)->wait_queue[index].fiber = NULL;
    waked_fiber->status = RUNABLE;
    (*f_rwlock)->wr_owner = waked_fiber;
    (*f_rwlock)->status = WR_LOCK;

    /* add the fiber to the scheduler running queue */
    struct ThreadCarrier *tc = waked_fiber->tc;
    uint16_t pos = __sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1);
    tc->running_queue.queue[pos] = waked_fiber;
    sem_post(&tc->running_queue.sem_used);
    return 0;
}
