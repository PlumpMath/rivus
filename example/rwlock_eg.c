#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/fiber.h"
#include "../include/fiber_mutex.h"
#include "../include/fiber_rwlock.h"

fiber_rwlock_t lock;
int g_value = 0;

void read_func(fiber_t fiber, void *data);
void write_func(fiber_t fiber, void *data);

int main() {
    fiber_rwlock_init(&lock);

    struct Scheduler *sch = create_scheduler(8);
    if (sch == NULL) return 0;
    start_io_dispatcher(sch);
    start_scheduler(sch);

    int i = 0;
    for (; i < 128; ++i) {
        fiber_t fiber;
        create_fiber(&fiber, write_func, NULL);
        schedule(sch, fiber);

        int j = 0;
        for (; j < 15; ++j) {
            create_fiber(&fiber, read_func, NULL);
            schedule(sch, fiber);
        }
    }

    stop_scheduler(sch);
    stop_io_dispatcher(sch);
    free_scheduler(sch);

    fiber_rwlock_destroy(&lock);
}

void write_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 256; ++i) {
        fiber_rwlock_wrlock(fiber, &lock);
        printf("writer: value is %d\n", ++g_value);
        fiber_rwlock_unlock(fiber, &lock);
    }
}

void read_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 256; ++i) {
        fiber_rwlock_rdlock(fiber, &lock);
        printf("reader: value is %d\n", g_value);
        fiber_rwlock_unlock(fiber, &lock);
    }
}
