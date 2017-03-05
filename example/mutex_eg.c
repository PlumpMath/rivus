#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/fiber.h"
#include "../include/fiber_mutex.h"

void func(fiber_t fiber, void *data);

fiber_mutex_t mtx;
int g_val_array[8] = {0};

int main() {
    fiber_mutex_init(&mtx);

    struct Scheduler *sch = create_scheduler(8);
    if (sch == NULL) return 0;
    start_scheduler(sch);
    start_io_dispatcher(sch);

    int i = 0;
    for (; i < 1024; ++i) {
        fiber_t fiber;
        create_fiber(&fiber, func, NULL);
        schedule(sch, fiber);
    }

    stop_scheduler(sch);
    stop_io_dispatcher(sch);
    free_scheduler(sch);

    fiber_mutex_destroy(&mtx);
}

void func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 64; ++i) {
        fiber_mutex_lock(fiber, &mtx);
        int j = 0;
        for (; j < 8; ++j) {
            printf("%d ", ++g_val_array[j]);
        }
        printf("\n");
        fiber_mutex_unlock(fiber, &mtx);
    }
}
