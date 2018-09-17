#pragma once

#include <stdlib.h>
#include <netdb.h>
#include <sys/epoll.h>

//==>assert macros<==
#define ensure(expr)\
    do {\
        if (!(expr)) {\
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__);\
            perror(#expr);\
            exit(1);\
        }\
    } while(0)

#define ensure_nonblock(expr)\
    do {\
        if (!(expr) && errno != EAGAIN) {\
            fprintf(stderr, "%s::%s::%d\n\t", __FILE__, __FUNCTION__, __LINE__);\
            perror(#expr);\
            exit(1);\
        }\
    } while(0)


void set_non_blocking(int sfd);
void make_storage(struct sockaddr_storage * addr, const char * host, int port);
int make_bound_udp(int port);

#define MAXEVENTS 256
//simplicity wrappers
#define EVENT_IN(events, i) (events[i].events & EPOLLIN)
#define EVENT_ERR(events, i) (events[i].events & EPOLLERR)
#define EVENT_HUP(events, i) (events[i].events & EPOLLHUP)
#define EVENT_OUT(events, i) (events[i].events & EPOLLIN)

#define EVENT_FD(events, i) (events[i].data.fd)
#define EVENT_PTR(events, i) (events[i].data.ptr)

int make_epoll();
struct epoll_event * make_epoll_events();
int wait_epoll(int efd, struct epoll_event * events);
int add_epoll_ptr(int efd, int ifd, void * ptr);
int add_epoll_fd(int efd, int ifd);

int extract_udp_slice(int sfd, struct sockaddr_storage* storage, uint8_t* slice);
