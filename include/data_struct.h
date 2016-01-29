#ifndef __DATA_STRUCT_H
#define __DATA_STRUCT_H

#include <semaphore.h>
#include <stdint.h>
#include <ucontext.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIBER_STACK_SIZE    4096*32
#define MAX_EVENT_SIZE  1024*64

typedef struct Fiber *fiber_t;

enum FIBER_STATUS {
    READY = 0,
    RUNABLE = 1,
    RUNNING = 2,
    SUSPEND = 3,
    DEAD = 4,
};

enum RWLOCK_STATUS {
    NO_LOCK = 0,
    RD_LOCK = 1,
    WR_LOCK = 2,
};

struct Fiber {
    void (*entry)(struct Fiber*);
    void (*user_func)(struct Fiber*, void*);
    void *user_data;
    int status;
    char stack[FIBER_STACK_SIZE];
    ucontext_t ctx;
    struct ThreadCarrier *tc;
};

struct FiberQueue {
    uint32_t size;
    volatile uint32_t head;
    uint32_t tail;
    sem_t sem_free;
    sem_t sem_used;
    struct Fiber **queue;
};

struct ThreadCarrier {
    pthread_t tid;
    struct Fiber *running_fiber;
    struct FiberQueue fiber_queue;
    struct Scheduler *sch;
    ucontext_t ctx;
    pthread_mutex_t mtx;
};

struct Scheduler {
    int thread_size;
    int index;
    struct ThreadCarrier **threads;
    int epoll_fd;
    pthread_t dispatcher_tid;
    struct Fiber **blocked_io_set;
};

struct FiberMutex {
    volatile uint32_t value;
    struct Fiber *owner;
    struct Fiber **wait_queue;
};

struct RWLockedFiber {
    int status;
    struct Fiber *fiber;
};

struct FiberRWLock {
    uint64_t value;
    struct Fiber *wr_owner;
    struct RWLockedFiber *wait_queue;
    int status;
};

struct FiberSemaphore {
    volatile uint32_t value;
    struct Fiber **wait_queue;
};

struct FiberCond {
    uint32_t value;
    struct Fiber **wait_queue;
};

struct TcpServer {
    int socket;
    void(*handle)(struct Fiber*, void*);
};

#ifdef __cplusplus
}
#endif
#endif