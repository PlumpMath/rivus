#include "sync_io.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#include "fiber.h"


extern int suspend_fiber(fiber_t fiber, int fd);


int rivus_open(const char *path, int flag) {
    return open(path, flag|O_NONBLOCK);
}

ssize_t rivus_read(fiber_t fiber, int fd, char *buf, size_t nbyte) {
    while (1) {
        int read_byte = read(fd, buf, nbyte);
        if (read_byte >= 0) {
            return read_byte;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            suspend_fiber(fiber, fd);

            int epoll_fd = fiber->tc->sch->epoll_fd;
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
            ev.data.fd = fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                return -1;
            }
            swapcontext(&fiber->ctx, &fiber->tc->ctx);
        } else {
            return -1;
        }
    }
}

ssize_t rivus_write(fiber_t fiber, int fd, char *buf, size_t nbyte) {
    while (1) {
        int write_byte = write(fd, buf, nbyte);
        if (write_byte >= 0) {
            return write_byte;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            suspend_fiber(fiber, fd);

            int epoll_fd = fiber->tc->sch->epoll_fd;
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
            ev.data.fd = fd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                return -1;
            }
            swapcontext(&fiber->ctx, &fiber->tc->ctx);
        } else {
            return -1;
        }
    }
}

int rives_close(int fd) {
    return close(fd);
}
