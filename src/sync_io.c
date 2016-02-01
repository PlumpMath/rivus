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

int rives_close(int fd) {
    return close(fd);
}

ssize_t rivus_write(fiber_t fiber, int fd, const char *buf, size_t nbyte) {
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

ssize_t rivus_writen(fiber_t fiber, int fd, const char *buf, size_t nbyte) {
    size_t rbyte = nbyte;
    const char *tbuf = buf;
    while (rbyte) {
        int rval = rivus_write(fiber, fd, tbuf, rbyte);
        if (rval <= 0) {
            return (nbyte - rbyte);
        }
        rbyte -= rval;
        tbuf += rval;
    }
    return nbyte;
}

ssize_t rivus_readn(fiber_t fiber, int fd, char *buf, size_t nbyte) {
    size_t rbyte = nbyte;
    char *tbuf = buf;
    while (rbyte) {
        int rval = rivus_read(fiber, fd, tbuf, rbyte);
        if (rval <= 0) {
            return (nbyte - rbyte);
        }
        rbyte -= rval;
        tbuf += rval;
    }
    return nbyte;
}

ssize_t rivus_send(fiber_t fiber, int fd, const char *buf, size_t len, int flags) {
    while (1) {
        int send_byte = send(fd, buf, len, flags);
        if (send_byte >= 0) {
            return send_byte;
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

ssize_t rivus_recv(fiber_t fiber, int fd, char *buf, size_t len, int flags) {
    while (1) {
        int recv_byte = recv(fd, buf, len, flags);
        if (recv_byte >= 0) {
            return recv_byte;
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

ssize_t rivus_sendto(fiber_t fiber, int sockfd, const char *buf, size_t len, int flags,
                     struct sockaddr *dest_addr, socklen_t *addrlen) {
    while (1) {
        int send_byte = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
        if (send_byte >= 0) {
            return send_byte;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            suspend_fiber(fiber, sockfd);

            int epoll_fd = fiber->tc->sch->epoll_fd;
            struct epoll_event ev;
            ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
            ev.data.fd = sockfd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
                return -1;
            }
            swapcontext(&fiber->ctx, &fiber->tc->ctx);
        } else {
            return -1;
        }
    }
}

ssize_t rivus_recvfrom(fiber_t fiber, int sockfd, char *buf, size_t len, int flags,
                       struct sockaddr *src_addr, socklen_t *addrlen) {
    while (1) {
        int recv_byte = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
        if (recv_byte >= 0) {
            return recv_byte;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            suspend_fiber(fiber, sockfd);

            int epoll_fd = fiber->tc->sch->epoll_fd;
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
            ev.data.fd = sockfd;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
                return -1;
            }
            swapcontext(&fiber->ctx, &fiber->tc->ctx);
        } else {
            return -1;
        }
    }
}
