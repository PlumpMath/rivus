#ifndef __FIBER_COND_H
#define __FIBER_COND_H

#include "data_struct.h"
#include "fiber_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FiberCond *fiber_cond_t;

int fiber_cond_init(fiber_cond_t *f_cond);
int fiber_cond_destroy(fiber_cond_t *f_cond);

int fiber_cond_signal(fiber_cond_t *f_cond);
int fiber_cond_broadcast(fiber_cond_t *f_cond);
int fiber_cond_wait(fiber_t fiber, fiber_cond_t *f_cond, fiber_mutex_t *f_mtx);

#ifdef __cplusplus
}
#endif
#endif
