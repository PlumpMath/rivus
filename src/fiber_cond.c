#include "fiber_cond.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <ucontext.h>

#include "fiber.h"

#define COND_WAIT_QUEUE_SIZE        1024*64
#define COND_WAIT_QUEUE_INDEX_MASK  0xffff
#define COND_WAIT_QUEUE_LEN_MASK    0xfffff

int fiber_cond_init(fiber_cond_t *f_cond) {
    if (f_cond == NULL) {
        return -1;
    }

    (*f_cond) = malloc(sizeof(struct FiberCond));
    (*f_cond)->value = 0;
    (*f_cond)->wait_queue = calloc(COND_WAIT_QUEUE_SIZE, sizeof(struct Fiber*));
    return 0;
}

int fiber_cond_destroy(fiber_cond_t *f_cond) {
    if (!f_cond || !(*f_cond) || ((*f_cond)->value & COND_WAIT_QUEUE_LEN_MASK) != 0) {
        return -1;
    }

    free((*f_cond)->wait_queue);
    free(*f_cond);
    return 0;
}

int fiber_cond_wait(fiber_t fiber, fiber_cond_t *f_cond, fiber_mutex_t *f_mtx) {
    uint64_t value = __sync_fetch_and_add(&(*f_cond)->value, 0x100000001);
    uint32_t len = value & COND_WAIT_QUEUE_LEN_MASK;

    assert(value < COND_WAIT_QUEUE_SIZE);

    fiber->status = SUSPEND;
    uint32_t index = (value >> 32) & COND_WAIT_QUEUE_INDEX_MASK;
    (*f_cond)->wait_queue[index] = fiber;

    fiber->tc->fiber_queue.queue[fiber->tc->fiber_queue.tail] = NULL;

    fiber_mutex_unlock(fiber, f_mtx);

    swapcontext(&fiber->ctx, &fiber->tc->ctx);

    fiber_mutex_lock(fiber, f_mtx);
    return 0;
}

int fiber_cond_signal(fiber_cond_t *f_cond) {
    uint32_t value, new_value;
    do {
        value = (*f_cond)->value;
        new_value = (value & 0xffffffff) ? (value - 1) : (value & 0xffffffff00000000);
    } while (!__sync_bool_compare_and_swap(&(*f_cond)->value, value, new_value));

    uint32_t len = value & COND_WAIT_QUEUE_LEN_MASK;
    if (len == 0) {
        return 0;
    }

    uint32_t index = ((value >> 32) - len) & COND_WAIT_QUEUE_INDEX_MASK;
    while ((*f_cond)->wait_queue[index] == NULL) {
        usleep(1);
    }
    fiber_t waked_fiber = (*f_cond)->wait_queue[index];
    (*f_cond)->wait_queue[index] = NULL;
    waked_fiber->status = RUNABLE;

    struct ThreadCarrier *tc = waked_fiber->tc;
    uint16_t pos = __sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1);
    tc->fiber_queue.queue[pos] = waked_fiber;
    sem_post(&tc->fiber_queue.sem_used);
    return 0;
}

int fiber_cond_broadcast(fiber_cond_t *f_cond) {
    uint64_t value = __sync_fetch_and_and(&(*f_cond)->value, 0xffffffff00000000);
    uint32_t len = value & COND_WAIT_QUEUE_LEN_MASK;

    uint32_t first_index = ((value >> 32) - len) & COND_WAIT_QUEUE_INDEX_MASK;
    size_t i = 0;
    for (; i < len; ++i) {
        uint32_t index = (first_index + i) & COND_WAIT_QUEUE_INDEX_MASK;
        while ((*f_cond)->wait_queue[index] == NULL) {
            usleep(1);
        }
        fiber_t waked_fiber = (*f_cond)->wait_queue[index];
        (*f_cond)->wait_queue[index] = NULL;
        waked_fiber->status = RUNABLE;

        struct ThreadCarrier *tc = waked_fiber->tc;
        uint16_t pos = __sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1);
        tc->fiber_queue.queue[pos] = waked_fiber;
        sem_post(&tc->fiber_queue.sem_used);
    }
    return 0;
}
