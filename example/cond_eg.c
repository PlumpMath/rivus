#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/fiber.h"
#include "../include/fiber_cond.h"
#include "../include/fiber_mutex.h"

fiber_mutex_t mtx;
fiber_cond_t cond;

int g_value = 0;

void write_func(fiber_t fiber, void *data);
void read_func(fiber_t fiber, void *data);

int main() {
    fiber_mutex_init(&mtx);
    fiber_cond_init(&cond);

    struct Scheduler *sch = create_scheduler(4);
    if (sch == NULL) return 0;
    start_scheduler(sch);
    start_io_dispatcher(sch);

    int i = 0;
    for (; i < 8; ++i) {
        fiber_t fiber;
        create_fiber(&fiber, read_func, NULL);
        schedule(sch, fiber);
    }

    for (i = 0; i < 8; ++i) {
        fiber_t fiber;
        create_fiber(&fiber, write_func, NULL);
        schedule(sch, fiber);
    }

    stop_scheduler(sch);
    stop_io_dispatcher(sch);
    free_scheduler(sch);

    fiber_cond_destroy(&cond);
    fiber_mutex_destroy(&mtx);
}

void write_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 128; ++i) {
        fiber_mutex_lock(fiber, &mtx);
        printf("writer value is %d\n", g_value);
        g_value = 64;
        fiber_cond_signal(&cond);
        // fiber_cond_broadcast(&cond);
        fiber_mutex_unlock(fiber, &mtx);
        yield(fiber);
    }
    printf("================================================ writer done!\n");
}

void read_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 16; ++i) {
        fiber_mutex_lock(fiber, &mtx);
        while (g_value < 32) {
            fiber_cond_wait(fiber, &cond, &mtx);
        }
        printf("reader value is %d\n", g_value);
        g_value = 0;
        fiber_mutex_unlock(fiber, &mtx);
        yield(fiber);
    }
    printf("================================================ reader done!\n");
}
