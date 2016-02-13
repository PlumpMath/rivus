#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/fiber.h"
#include "../include/fiber_semaphore.h"

fiber_sem_t sem;
int g_value = 0;

void consume_func(fiber_t fiber, void *data);
void product_func(fiber_t fiber, void *data);

int main() {
    fiber_sem_init(&sem, 0);

    struct Scheduler *sch = create_scheduler(4);
    if (sch == NULL) return 0;
    start_scheduler(sch);
    start_io_dispatcher(sch);

    int i = 0;
    for (; i < 256; ++i) {
        fiber_t fiber;

        create_fiber(&fiber, consume_func, NULL);
        schedule(sch, fiber);

        create_fiber(&fiber, product_func, NULL);
        schedule(sch, fiber);
    }

    stop_scheduler(sch);
    stop_io_dispatcher(sch);
    free_scheduler(sch);

    fiber_sem_destroy(&sem);
}

void consume_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 128; ++i) {
        fiber_sem_wait(fiber, &sem);
        printf("consume: value is %d\n", __sync_sub_and_fetch(&g_value, 1));
        yield(fiber);
    }
}

void product_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 128; ++i) {
        fiber_sem_post(&sem);
        printf("product: value is %d\n", __sync_add_and_fetch(&g_value, 1));
        yield(fiber);
    }
}
