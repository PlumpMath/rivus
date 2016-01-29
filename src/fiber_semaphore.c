#include "fiber_semaphore.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#include "fiber.h"

#define SEM_VALUE_MAX     1024*4
#define SEM_VALUE_MASK      0x1fff
#define SEM_WAIT_QUEUE_SIZE     SEM_VALUE_MAX
#define SEM_WAIT_QUEUE_LEN_MASK     0xfff

int fiber_sem_init(fiber_sem_t *f_sem, int value) {
    if (f_sem == NULL ||
        value < 0 || value >= SEM_VALUE_MAX) {
        return -1;
    }

    (*f_sem) = malloc(sizeof(struct FiberSemaphore));
    uint16_t tval = 0x1000 - value;
    (*f_sem)->value = (tval << 16) + tval;
    (*f_sem)->wait_queue = calloc(SEM_WAIT_QUEUE_SIZE, sizeof(struct Fiber*));
    return 0;
}

int fiber_sem_destroy(fiber_sem_t *f_sem) {
    if (!(*f_sem) || ((*f_sem)->value & SEM_VALUE_MASK) != 0x1000) {
        return -1;
    }

    free((*f_sem)->wait_queue);
    free(*f_sem);
    return 0;
}

int fiber_sem_wait(fiber_t fiber, fiber_sem_t *f_sem) {
    uint32_t value = __sync_fetch_and_add(&(*f_sem)->value, 0x10001);
    uint16_t val = value & SEM_VALUE_MASK;

    assert(val != 0x1fff);

    if (val < 0x1000) {
        return 0;
    }

    fiber->status = SUSPEND;
    uint16_t index = (value >> 16) & SEM_WAIT_QUEUE_LEN_MASK;

    (*f_sem)->wait_queue[index] = fiber;

    fiber->tc->fiber_queue.queue[fiber->tc->fiber_queue.tail] = NULL;
    swapcontext(&fiber->ctx, &fiber->tc->ctx);
    return 0;
}

int fiber_sem_post(fiber_sem_t *f_sem) {
    uint32_t value = __sync_sub_and_fetch(&(*f_sem)->value, 0x1);
    uint16_t val = value & SEM_VALUE_MASK;
    assert(val != 0x0);

    if (val < 0x1000) {
        return 0;
    }

    uint16_t index = ((value >> 16) - val - 1) & SEM_WAIT_QUEUE_LEN_MASK;
    while ((*f_sem)->wait_queue[index] == NULL) {
        usleep(1);
    }
    fiber_t waked_fiber = (*f_sem)->wait_queue[index];
    (*f_sem)->wait_queue[index] = NULL;
    waked_fiber->status = RUNABLE;

    struct ThreadCarrier *tc = waked_fiber->tc;
    uint16_t pos = __sync_fetch_and_add(&tc->fiber_queue.head, 1) & (tc->fiber_queue.size - 1);
    tc->fiber_queue.queue[pos] = waked_fiber;
    sem_post(&tc->fiber_queue.sem_used);
    return 0;
}

int fiber_sem_getvalue(fiber_sem_t *f_sem, int *sval) {
    if (!f_sem || !(*f_sem)) {
        return -1;
    }

    *sval = ((*f_sem)->value & 0x1000) ? -((*f_sem)->value & SEM_WAIT_QUEUE_LEN_MASK)
        : (0x1000 - ((*f_sem)->value & SEM_VALUE_MASK));
    return 0;
}
