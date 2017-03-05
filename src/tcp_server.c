#include "tcp_server.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "fiber.h"


#define MAX_CONNECTION  1024*64


extern int wake_fiber(struct Scheduler *sch, int fd);


struct TcpServer* create_tcp_server(const char* ip, int port, void(*handle)(fiber_t, void*)) {
    struct sockaddr_in server_address;
    in_addr_t server_ip;
    int optval = 0;
    struct TcpServer *server = calloc(1, sizeof(struct TcpServer));

    if (!ip) {
        server_ip = htonl(INADDR_ANY);
    } else if ((server_ip = inet_addr(ip)) == INADDR_NONE) {
        return NULL;
    }

    if (handle == NULL) {
        return NULL;
    }

    if ((server->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return NULL;
    }

    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return NULL;
    }

    if (fcntl(server->socket, F_SETFL, fcntl(server->socket, F_GETFL, 0)|O_NONBLOCK) < 0) {
        return NULL;
    }

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = server_ip;
    server_address.sin_port = htons(port);

    if (bind(server->socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        return NULL;
    }

    server->handle = handle;
    return server;
}

void free_tcp_server(struct TcpServer *server) {
    close(server->socket);
    free(server);
}

void run_tcp_server(struct Scheduler *sch, struct TcpServer *server) {
    struct epoll_event ev;
    struct epoll_event *events;
    struct sockaddr_in remote;
    socklen_t remote_len;
    int nfds;

    assert(listen(server->socket, MAX_CONNECTION) == 0);

    assert((sch->epoll_fd = epoll_create(32)) >= 0);

    ev.events = EPOLLIN;
    ev.data.fd = server->socket;
    assert(epoll_ctl(sch->epoll_fd, EPOLL_CTL_ADD, server->socket, &ev) >= 0);

    remote_len = sizeof(remote);
    events = calloc(MAX_EVENT_SIZE, sizeof(struct epoll_event));
    while (1) {
        nfds = epoll_wait(sch->epoll_fd, events, MAX_EVENT_SIZE, -1);

        int i = 0;
        for (; i < nfds; ++i) {
            if (events[i].data.fd == server->socket) {
                int *client_socket = malloc(sizeof(int));
                *client_socket = accept(server->socket,
                                        (struct sockaddr*)&remote, &remote_len);

                if (*client_socket < 0) {
                    printf("accept client connection fail!\n");
                    free(client_socket);
                    continue;
                }

                if (fcntl(*client_socket, F_SETFL,
                          fcntl(*client_socket, F_GETFL, 0)|O_NONBLOCK) < 0) {
                    printf("set client socket as nonblocking fail!\n");
                    close(*client_socket);
                    free(client_socket);
                    continue;
                }

                fiber_t fiber;
                create_fiber(&fiber, server->handle, (void*)client_socket);
                schedule(sch, fiber);
            } else {
                epoll_ctl(sch->epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                wake_fiber(sch, events[i].data.fd);
            }
        }
    }

    free(events);
    close(sch->epoll_fd);
}
