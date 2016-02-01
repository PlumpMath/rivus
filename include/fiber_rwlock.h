#ifndef __FIBER_RWLOCK_H
#define __FIBER_RWLOCK_H

#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FiberRWLock *fiber_rwlock_t;

int fiber_rwlock_init(fiber_rwlock_t *f_rwlock);
int fiber_rwlock_destroy(fiber_rwlock_t *f_rwlock);

int fiber_rwlock_rdlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);
int fiber_rwlock_wrlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);
int fiber_rwlock_unlock(fiber_t fiber, fiber_rwlock_t *f_rwlock);

#ifdef __cplusplus
}
#endif
#endif
