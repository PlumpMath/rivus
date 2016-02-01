#ifndef __FIBER_SEMAPHORE_H
#define __FIBER_SEMAPHORE_H

#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FiberSemaphore *fiber_sem_t;

int fiber_sem_init(fiber_sem_t *f_sem, int value);
int fiber_sem_destroy(fiber_sem_t *f_sem);

int fiber_sem_post(fiber_sem_t *f_sem);
int fiber_sem_wait(fiber_t fiber, fiber_sem_t *f_sem);

int fiber_sem_getvalue(fiber_sem_t *f_sem, int *sval);

#ifdef __cplusplus
}
#endif
#endif
