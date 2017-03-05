#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "../include/sync_io.h"
#include "../include/fiber.h"
#include "../include/fiber_mutex.h"
#include "../include/tcp_server.h"

void data_processor(fiber_t fiber, void *data);

int sharedArr[16];
fiber_mutex_t mtx;

int main() {
    struct rlimit rl;
    rl.rlim_max = rl.rlim_cur = 1024*64;
    if (setrlimit(RLIMIT_NOFILE, &rl) < -1) {
        return -1;
    }

    fiber_mutex_init(&mtx);
    memset((char*)sharedArr, 0, 64);

    const char *server_ip = "127.0.0.1";

    struct Scheduler *sch = create_scheduler(8);
    if (sch == NULL) {
        return 0;
    }

    struct TcpServer *server = create_tcp_server(server_ip, 12400, data_processor);
    if (server == NULL) {
        return 0;
    }

    start_scheduler(sch);
    run_tcp_server(sch, server);

    return 0;
}

void data_processor(fiber_t fiber, void *data) {
    int i = 0;
    int fd = *(int*)data;
    char *rbuf = malloc(10);
    char sbuf[5] = "fine";
    char *tbuf;
    int tlen, wrlen;

    while (1) {
        tlen = 10;
        tbuf = rbuf;
        memset(rbuf, 0, 10);
        while (tlen > 0) {
            int wrlen = rivus_read(fiber, fd, tbuf, tlen);
            if (wrlen == 0) {
                close(fd);
                return;
            }
            tlen -= wrlen;
            tbuf += wrlen;
        }

        /*
        fiber_mutex_lock(fiber, &mtx);
        int i = 0;
        // printf("array: ");
        for (; i < 16; ++i) {
            ++sharedArr[i];
            // printf(" %d", ++sharedArr[i]);
        }
        // printf("\n");
        fiber_mutex_unlock(fiber, &mtx);
        */

        tlen = 5;
        tbuf = sbuf;
        while (tlen > 0) {
            int wrlen = rivus_write(fiber, fd, tbuf, tlen);
            if (wrlen < 0) {
                close(fd);
                return;
            }
            tlen -= wrlen;
            tbuf += wrlen;
        }
        ++i;
    }
    free(rbuf);
}
