#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "network.h"

struct epoll_event *make_epoll_events() {
    return (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAXEVENTS);
}

void set_non_blocking(int sfd) {
    int flags;
    ensure((flags = fcntl(sfd, F_GETFL, 0)) != -1);
    flags |= O_NONBLOCK;
    ensure(fcntl(sfd, F_SETFL, flags) != -1);
}

void make_storage(struct sockaddr_storage *restrict addr, const char *restrict host, int port) {
    struct addrinfo hints;
    struct addrinfo *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags    = AI_PASSIVE; // All interfaces

    //null the service as it only accepts strings and we have the port already
    ensure(getaddrinfo(host, NULL, &hints, &rp) == 0);

    //assuming the first result returned will be correct
    //TODO find a way to check
    ensure(rp);

    //add the port manually
    if (rp->ai_family == AF_INET) {
        ((struct sockaddr_in *)rp->ai_addr)->sin_port = htons(port);
    } else if (rp->ai_family == AF_INET6) {
        ((struct sockaddr_in6 *)rp->ai_addr)->sin6_port = htons(port);
    }

    memcpy(addr, rp->ai_addr, rp->ai_addrlen);

    freeaddrinfo(rp);
}

int make_bound_udp(int port) {
    struct sockaddr_in sin;
    int sockfd;

    ensure((sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) != -1);

    memset(&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port        = htons(port);
    sin.sin_family      = AF_INET;

    ensure(bind(sockfd, (struct sockaddr *)&sin, sizeof(sin)) != -1);

    return sockfd;
}

int make_epoll() {
    int efd;
    ensure((efd = epoll_create1(EPOLL_CLOEXEC)) != -1);
    return efd;
}

int wait_epoll(int efd, struct epoll_event *restrict events) {
    int ret;
    ensure((ret = epoll_wait(efd, events, MAXEVENTS, -1)) != -1);
    return ret;
}

int add_epoll_ptr(int efd, int ifd, void *ptr) {
    int ret;
    static struct epoll_event event;
    event.data.ptr = ptr;
    event.events   = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int add_epoll_fd(int efd, int ifd) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = EPOLLOUT | EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

int extract_udp_slice(int sfd, struct sockaddr_storage *restrict storage, uint8_t *restrict slice) {
    static char buffer[UDP_SLICE];
    static socklen_t len = sizeof(struct sockaddr_storage);
    ensure(recvfrom(sfd, buffer, UDP_SLICE, 0, (struct sockaddr *)storage, &len) != -1);
    if (storage->ss_family == AF_INET6) {
        return 1; //we cant handle V6 yet
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)storage;
    //as the slice is two bytes use a uint16_t to copy both
    uint16_t *wslice = (uint16_t *)slice;
    *wslice          = addr->sin_port;

    return 0;
}

int insert_udp_slice(struct sockaddr_storage *restrict storage, uint8_t *restrict slice) {
    static char buffer[UDP_SLICE];
    static socklen_t len = sizeof(struct sockaddr_storage);

    int sfd = make_bound_udp(*(uint16_t *)slice);

    ensure(sendto(sfd, buffer, 0, 0, (struct sockaddr *)storage, len) != -1);

    close(sfd);

    return 0;
}
