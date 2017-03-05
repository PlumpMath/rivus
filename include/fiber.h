#ifndef __FIBER_H
#define __FIBER_H

#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Scheduler* create_scheduler(int thread_num);
void free_scheduler(struct Scheduler *sch);

void start_scheduler(struct Scheduler *sch);
void stop_scheduler(struct Scheduler *sch);

void start_io_dispatcher(struct Scheduler *sch);
void stop_io_dispatcher(struct Scheduler *sch);

void switch_to_scheduler(fiber_t fiber);
void switch_to_fiber(fiber_t fiber);

int create_fiber(fiber_t *fiber, void (*user_func)(struct Fiber*, void*), void *data);
int schedule(struct Scheduler *sch, fiber_t fiber);
int yield(fiber_t fiber);

#ifdef __cplusplus
}
#endif
#endif
