#ifndef __FIBER_MUTEX_H
#define __FIBER_MUTEX_H

#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FiberMutex *fiber_mutex_t;

int fiber_mutex_init(fiber_mutex_t *f_mtx);
int fiber_mutex_destroy(fiber_mutex_t *f_mtx);

int fiber_mutex_lock(fiber_t fiber, fiber_mutex_t *f_mtx);
int fiber_mutex_unlock(fiber_t fiber, fiber_mutex_t *f_mtx);

#ifdef __cplusplus
}
#endif
#endif
