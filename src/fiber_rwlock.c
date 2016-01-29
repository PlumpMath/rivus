#include "fiber_rwlock.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <ucontext.h>

#include "fiber.h"

#define RWLOCK_WAIT_QUEUE_SIZE  4096
#define RWLOCK_WAIT_QUEUE_LEN_MASK  0xfff

static int wr_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);
static int rd_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);

int fiber_rwlock_init(fiber_rwlock_t *f_rwlock) {
    if (f_rwlock == NULL) {
        return -1;
    }

    (*f_rwlock) = malloc(sizeof(struct FiberRWLock));
    (*f_rwlock)->value = 0x0;
    (*f_rwlock)->wr_owner = NULL;
    (*f_rwlock)->status = 0;
    (*f_rwlock)->wait_queue = calloc(RWLOCK_WAIT_QUEUE_SIZE, sizeof(struct RWLockedFiber));
    return 0;
}

int fiber_rwlock_destroy(fiber_rwlock_t *f_rwlock) {
    if (!(*f_rwlock) ||
        ((*f_rwlock)->value & RWLOCK_WAIT_QUEUE_LEN_MASK & (RWLOCK_WAIT_QUEUE_LEN_MASK << 16)) != 0x0) {
        return -1;
    }

    free((*f_rwlock)->wait_queue);
    free(*f_rwlock);
    return 0;
}

int fiber_rwlock_rdlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    uint64_t value = __sync_fetch_and_add(&(*f_rwlock)->value, 0x100000001);
    uint16_t rd_len = (value) & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint16_t wr_len = (value >> 16) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert(rd_len != (RWLOCK_WAIT_QUEUE_SIZE - 1));

    uint16_t index = (value >> 32) & RWLOCK_WAIT_QUEUE_LEN_MASK;
    (*f_rwlock)->wait_queue[index].status = RD_LOCK;

    if (wr_len == 0) {
        if (rd_len == 0) {
            (*f_rwlock)->wait_queue[index].status = NO_LOCK;
            (*f_rwlock)->status = RD_LOCK;
        }
        return 0;
    }

    fiber->status = SUSPEND;
    (*f_rwlock)->wait_queue[index].fiber = fiber;

    fiber->tc->fiber_queue.queue[fiber->tc->fiber_queue.tail] = NULL;
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
    return 0;
}

int fiber_rwlock_wrlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    /* nested lock */
    assert(fiber != (*f_rwlock)->wr_owner);

    uint64_t value = __sync_fetch_and_add(&(*f_rwlock)->value, 0x100010000);
    uint16_t rd_len = value & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint16_t wr_len = (value >> 16) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert(wr_len != (RWLOCK_WAIT_QUEUE_SIZE - 1));

    if (rd_len == 0 && wr_len == 0) {
        (*f_rwlock)->status = WR_LOCK;
        (*f_rwlock)->wr_owner = fiber;
        return 0;
    }

    fiber->status = SUSPEND;
    uint16_t index = (value >> 32) & RWLOCK_WAIT_QUEUE_LEN_MASK;
    (*f_rwlock)->wait_queue[index].status = WR_LOCK;
    (*f_rwlock)->wait_queue[index].fiber = fiber;

    fiber->tc->fiber_queue.queue[fiber->tc->fiber_queue.tail] = NULL;
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
    uint64_t value = __sync_sub_and_fetch(&(*f_rwlock)->value, 0x10000);
    uint16_t rd_len = value & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint16_t wr_len = (value >> 16) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert(wr_len != (RWLOCK_WAIT_QUEUE_SIZE - 1));

    if (wr_len == 0 && rd_len == 0) {
        (*f_rwlock)->status = NO_LOCK;
        return 0;
    }

    uint16_t index = ((value >> 32) - wr_len - rd_len) & RWLOCK_WAIT_QUEUE_LEN_MASK;
    while ((*f_rwlock)->wait_queue[index].fiber == NULL) {
        usleep(1);
    }
    if ((*f_rwlock)->wait_queue[index].status == WR_LOCK) {
        fiber_t waked_fiber = (*f_rwlock)->wait_queue[index].fiber;
        (*f_rwlock)->wait_queue[index].status = NO_LOCK;
        (*f_rwlock)->wait_queue[index].fiber = NULL;
        waked_fiber->status = RUNABLE;
        (*f_rwlock)->wr_owner = waked_fiber;

        struct ThreadCarrier *tc = waked_fiber->tc;
        uint16_t pos = __sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1);
        tc->fiber_queue.queue[pos] = waked_fiber;
        sem_post(&tc->fiber_queue.sem_used);
    } else {
        (*f_rwlock)->status = RD_LOCK;
        int i = 0;
        for (; i < rd_len; ++i) {
            int t = (index + i) & RWLOCK_WAIT_QUEUE_LEN_MASK;
            while ((*f_rwlock)->wait_queue[t].fiber == NULL) {
                usleep(1);
            }
            if ((*f_rwlock)->wait_queue[t].status == WR_LOCK) {
                break;
            }

            fiber_t waked_fiber = (*f_rwlock)->wait_queue[t].fiber;
            (*f_rwlock)->wait_queue[t].fiber = NULL;
            waked_fiber->status = RUNABLE;

            struct ThreadCarrier *tc = waked_fiber->tc;
            uint16_t pos = __sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1);
            tc->fiber_queue.queue[pos] = waked_fiber;
            sem_post(&tc->fiber_queue.sem_used);
        }
    }
    return 0;
}

int rd_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock) {
    uint64_t value = __sync_sub_and_fetch(&(*f_rwlock)->value, 0x1);
    uint16_t rd_len = value & RWLOCK_WAIT_QUEUE_LEN_MASK;
    uint16_t wr_len = (value >> 16) & RWLOCK_WAIT_QUEUE_LEN_MASK;

    assert(rd_len != (RWLOCK_WAIT_QUEUE_SIZE - 1));

    if (wr_len == 0 && rd_len == 0) {
        (*f_rwlock)->status = NO_LOCK;
        return 0;
    }

    uint16_t index = ((value >> 32) - wr_len - rd_len) & RWLOCK_WAIT_QUEUE_LEN_MASK;
    while ((*f_rwlock)->wait_queue[index].status == NO_LOCK) {
        usleep(1);
    }
    if ((*f_rwlock)->wait_queue[index].status == RD_LOCK) {
        (*f_rwlock)->wait_queue[index].status = NO_LOCK;
        return 0;
    }

    fiber_t waked_fiber = (*f_rwlock)->wait_queue[index].fiber;
    (*f_rwlock)->wait_queue[index].status = NO_LOCK;
    (*f_rwlock)->wait_queue[index].fiber = NULL;
    waked_fiber->status = RUNABLE;
    (*f_rwlock)->wr_owner = waked_fiber;

    (*f_rwlock)->status = WR_LOCK;

    struct ThreadCarrier *tc = waked_fiber->tc;
    uint16_t pos = __sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1);
    tc->fiber_queue.queue[pos] = waked_fiber;
    sem_post(&tc->fiber_queue.sem_used);
    return 0;
}
