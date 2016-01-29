#ifndef __TCP_SERVER_H
#define __TCP_SERVER_H

#include "data_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TcpServer* create_tcp_server(const char* ip, int port,
                                    void(*handle)(struct Fiber*, void*));
void free_tcp_server(struct TcpServer *server);
void run_tcp_server(struct Scheduler *sch, struct TcpServer *server);

#ifdef __cplusplus
}
#endif
#endif
