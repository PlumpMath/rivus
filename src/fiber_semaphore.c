#include "fiber_semaphore.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "fiber.h"


#define SEM_VALUE_MAX       0xffe0000
#define SEM_VALUE_MASK      0xffffffff
#define SEM_WAIT_QUEUE_SIZE     1024*64
#define SEM_WAIT_QUEUE_INDEX_MASK     0xffff


int fiber_sem_init(fiber_sem_t *f_sem, int value) {
    if (f_sem == NULL || value < 0 || value >= SEM_VALUE_MAX) {
        return -1;
    }

    (*f_sem) = malloc(sizeof(struct FiberSemaphore));
    uint64_t tval = SEM_VALUE_MAX - value;
    (*f_sem)->value = (tval << 32) + tval;
    (*f_sem)->wait_queue = calloc(SEM_WAIT_QUEUE_SIZE, sizeof(struct Fiber*));
    return 0;
}

int fiber_sem_destroy(fiber_sem_t *f_sem) {
    if (!(*f_sem) || ((*f_sem)->value & SEM_VALUE_MASK) != SEM_VALUE_MAX) {
        return -1;
    }

    free((*f_sem)->wait_queue);
    free(*f_sem);
    return 0;
}

int fiber_sem_wait(fiber_t fiber, fiber_sem_t *f_sem) {
    uint64_t value = __sync_fetch_and_add(&(*f_sem)->value, 0x100000001);
    uint32_t val = value & SEM_VALUE_MASK;

    assert(val < SEM_VALUE_MAX + 0x10000);

    if (val < SEM_VALUE_MAX) {
        /* value of the semaphore is greater than zero, return */
        return 0;
    }

    /* suspend the fiber, and put it on the wait queue */
    fiber->status = SUSPEND;
    uint32_t index = (value >> 32) & SEM_WAIT_QUEUE_INDEX_MASK;

    (*f_sem)->wait_queue[index] = fiber;

    fiber->tc->running_queue.queue[fiber->tc->running_queue.tail] = NULL;
    switch_to_scheduler(fiber);
    return 0;
}

int fiber_sem_post(fiber_sem_t *f_sem) {
    uint64_t value = __sync_sub_and_fetch(&(*f_sem)->value, 0x1);
    uint32_t val = value & SEM_VALUE_MASK;

    assert(val < SEM_VALUE_MAX + 0x10000);

    if (val < SEM_VALUE_MAX) {
        /* no fiber is on the wait queue, return immediately */
        return 0;
    }

    /* get the wait queue tail index, the fiber must be woke up was put on there */
    uint32_t index = ((value >> 32) - val - 1) & SEM_WAIT_QUEUE_INDEX_MASK;
    while ((*f_sem)->wait_queue[index] == NULL) {
        usleep(1);
    }

    /* remove the fiber from the wait queue */
    fiber_t waked_fiber = (*f_sem)->wait_queue[index];
    (*f_sem)->wait_queue[index] = NULL;
    waked_fiber->status = RUNABLE;

    struct ThreadCarrier *tc = waked_fiber->tc;
    uint16_t pos = __sync_fetch_and_add(&tc->running_queue.head, 1) & (tc->running_queue.size - 1);
    tc->running_queue.queue[pos] = waked_fiber;
    sem_post(&tc->running_queue.sem_used);
    return 0;
}

int fiber_sem_getvalue(fiber_sem_t *f_sem, int *sval) {
    if (!f_sem || !(*f_sem)) {
        return -1;
    }

    uint32_t value = (*f_sem)->value;
    *sval = (value & SEM_VALUE_MASK) > SEM_VALUE_MAX ?
        -(value & SEM_WAIT_QUEUE_INDEX_MASK) : (SEM_VALUE_MAX - (value & SEM_VALUE_MASK));
    return 0;
}
