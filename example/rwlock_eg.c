#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../include/fiber.h"
#include "../include/fiber_mutex.h"
#include "../include/fiber_semaphore.h"
#include "../include/fiber_rwlock.h"

fiber_rwlock_t lock;
int g_value = 0;

void read_func(fiber_t fiber, void *data);
void write_func(fiber_t fiber, void *data);

int main() {
    struct Scheduler *sch = create_scheduler(4);
    if (sch == NULL) return 0;
    start_scheduler(sch);
    start_io_dispatcher(sch);

    fiber_rwlock_init(&lock);

    int i = 0;
    for (; i < 8; ++i) {
        fiber_t fiber;
        create_fiber(&fiber, write_func, NULL);
        schedule(sch, fiber);
    }

    for (i = 0; i < 8; ++i) {
        fiber_t fiber;
        create_fiber(&fiber, read_func, NULL);
        schedule(sch, fiber);
    }

    stop_scheduler(sch);
    stop_io_dispatcher(sch);
    fiber_rwlock_destroy(&lock);
}

void write_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 256; ++i) {
        fiber_rwlock_wrlock(fiber, &lock);
        printf("writer: value is %d\n", __sync_add_and_fetch(&g_value, 1));
        fiber_rwlock_unlock(fiber, &lock);
        usleep(1);
    }
    printf("========================================= writer done!\n");
}

void read_func(fiber_t fiber, void *data) {
    int i = 0;
    for (; i < 256; ++i) {
        fiber_rwlock_rdlock(fiber, &lock);
        printf("reader: value is %d\n", g_value);
        fiber_rwlock_unlock(fiber, &lock);
    }
    printf("========================================= reader done!\n");
}