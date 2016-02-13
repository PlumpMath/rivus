#ifndef __DATA_STRUCT_H
#define __DATA_STRUCT_H

#include <semaphore.h>
#include <stdint.h>
#include <ucontext.h>


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

enum RWLOCK_WAIT_STATUS {
    NO_WAIT = 0,
    RD_WAIT = 1,
    WR_WAIT = 2,
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
    int stop;
    sem_t done;
    pthread_t tid;
    struct Fiber *running_fiber;
    struct FiberQueue running_queue;
    struct Scheduler *sch;
    ucontext_t ctx;
};

struct Scheduler {
    int stop;
    int thread_size;
    int index;
    int epoll_fd;
    pthread_t dispatcher_tid;
    struct ThreadCarrier **threads;
    struct Fiber **blocked_io_set;
};

struct FiberMutex {
    volatile uint64_t value;
    struct Fiber *owner;
    struct Fiber **wait_queue;
};

struct RWLockedFiber {
    int status;
    struct Fiber *fiber;
};

struct FiberRWLock {
    volatile uint64_t value;
    struct Fiber *wr_owner;
    struct RWLockedFiber *wait_queue;
    int status;
    int first_wr_index;
    int rd_count;
};

struct FiberSemaphore {
    volatile uint64_t value;
    struct Fiber **wait_queue;
};

struct FiberCond {
    volatile uint64_t value;
    struct Fiber **wait_queue;
};

struct TcpServer {
    int socket;
    void(*handle)(struct Fiber*, void*);
};

#endif
