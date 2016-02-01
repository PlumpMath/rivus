#ifndef __SYNC_IO_H
#define __SYNC_IO_H

#include <sys/socket.h>
#include <sys/types.h>

#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RIVUS_RDONLY    O_RDONLY
#define RIVUS_WRONLY    O_WRONLY
#define RIVUS_RDWR      O_RDWR
#define RIVUS_APPEND    O_APPEND
#define RIVUS_CREAT     O_CREAT

int rivus_open(const char *path, int flag);
int rives_close(int fd);

ssize_t rivus_write(fiber_t fiber, int fd, const char *buf, size_t nbyte);
ssize_t rivus_read(fiber_t fiber, int fd, char *buf, size_t nbyte);

ssize_t rivus_writen(fiber_t fiber, int fd, const char *buf, size_t nbyte);
ssize_t rivus_readn(fiber_t fiber, int fd, char *buf, size_t nbyte);

ssize_t rivus_send(fiber_t fiber, int sockfd, const char *buf, size_t len, int flags);
ssize_t rivus_recv(fiber_t fiber, int sockfd, char *buf, size_t len, int flags);

ssize_t rivus_sendto(fiber_t fiber, int sockfd, const char *buf, size_t len, int flags,
                     struct sockaddr *dest_addr, socklen_t *addrlen);
ssize_t rivus_recvfrom(fiber_t fiber, int sockfd, char *buf, size_t len, int flags,
                       struct sockaddr *src_addr, socklen_t *addrlen);

#ifdef __cplusplus
}
#endif
#endif
