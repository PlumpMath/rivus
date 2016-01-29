#ifndef __SYNC_IO_H
#define __SYNC_IO_H

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
ssize_t rivus_read(fiber_t fiber, int fd, char *buf, size_t nbyte);
ssize_t rivus_write(fiber_t fiber, int fd, char *buf, size_t nbyte);
int rives_close(int fd);

#ifdef __cplusplus
}
#endif
#endif
