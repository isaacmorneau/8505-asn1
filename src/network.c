#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "network.h"

struct pseudo_header {
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t udp_length;
};


/*
 * function:
 *    csum
 *
 * return:
 *    uint16_t the checksum
 *
 * parameters:
 *    uint16_t *ptr the buffer to check
 *    size_t nbytes the length of the buffer
 *
 * notes:
 *    standard checksum for IP
 *
 * */

uint16_t csum(uint16_t *ptr, size_t nbytes) {
    register int64_t sum;
    uint16_t oddbyte;
    register int16_t answer;
    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte                = 0;
        *((uint8_t *)&oddbyte) = *(uint8_t *)ptr;
        sum += oddbyte;
    }

    sum    = (sum >> 16) + (sum & 0xffff);
    sum    = sum + (sum >> 16);
    answer = (int16_t)~sum;

    return answer;
}

/*
 * function:
 *    make_epoll_events
 *
 * return:
 *    struct epoll_event* the events to use with epoll
 *
 * parameters:
 *    void
 *
 * notes:
 *    helper to make the events
 *
 * */
struct epoll_event *make_epoll_events(void) {
    return (struct epoll_event *)malloc(sizeof(struct epoll_event) * MAXEVENTS);
}


/*
 * function:
 *    set_non_blocking
 *
 * return:
 *    void
 *
 * parameters:
 *    int sfd the socket to operate on
 *
 * notes:
 *    makes a socket nonblocking
 *
 * */

void set_non_blocking(int sfd) {
    int flags;
    ensure((flags = fcntl(sfd, F_GETFL, 0)) != -1);
    flags |= O_NONBLOCK;
    ensure(fcntl(sfd, F_SETFL, flags) != -1);
}


/*
 * function:
 *    make_storage
 *
 * return:
 *    void
 *
 * parameters:
 *    struct sockaddr_storage *restrict addr the address to fill
 *    const char *restrict host the host to check
 *    int port the port to connect to
 *
 * notes:
 *    fills out a modern storage struct
 *
 * */

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


/*
 * function:
 *    make_bound_udp
 *
 * return:
 *    int the udp socket
 *
 * parameters:
 *    int port the port to bind to
 *
 * notes:
 *    make and bind to a port with udp
 *
 * */

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


/*
 * function:
 *    make_epoll
 *
 * return:
 *    int the epoll file descriptor
 *
 * parameters:
 *    void
 *
 * notes:
 *    wrap the creation of epoll instances
 *
 * */

int make_epoll() {
    int efd;
    ensure((efd = epoll_create1(EPOLL_CLOEXEC)) != -1);
    return efd;
}


/*
 * function:
 *    wait_epoll
 *
 * return:
 *    int number of events
 *
 * parameters:
 *    int efd epoll fd
 *    struct epoll_event *restrict events events to populate
 *
 * notes:
 *    wrap waiting for events
 *
 * */

int wait_epoll(int efd, struct epoll_event *restrict events) {
    int ret;
    ensure((ret = epoll_wait(efd, events, MAXEVENTS, -1)) != -1);
    return ret;
}


/*
 * function:
 *    add_epoll_fd
 *
 * return:
 *    int 1 for error
 *
 * parameters:
 *    int efd the epoll fd
 *    int ifd the fd to add
 *
 * notes:
 *    wrap adding new fds
 *
 * */

int add_epoll_fd(int efd, int ifd) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = EPOLLIN | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}


/*
 * function:
 *    extract_udp_slice
 *
 * return:
 *    int 1 for error
 *
 * parameters:
 *    int sfd the socket fd
 *    struct sockaddr_storage *restrict storage the storage to fill
 *    uint8_t *restrict slice the buffer to fill
 *
 * notes:
 *    pull the data from the udp packet
 *
 * */

int extract_udp_slice(int sfd, struct sockaddr_storage *restrict storage, uint8_t *restrict slice) {
    static char buffer[UDP_SLICE];
    static socklen_t len = sizeof(struct sockaddr_storage);
    ensure_nonblock(recvfrom(sfd, buffer, UDP_SLICE, 0, (struct sockaddr *)storage, &len) != -1);
    if (errno == EAGAIN || storage->ss_family == AF_INET6) {
        return 1; //we cant handle V6 yet
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)storage;
    //as the slice is two bytes use a uint16_t to copy both
    uint16_t *wslice = (uint16_t *)slice;
    *wslice          = ntohs(addr->sin_port);

    return 0;
}

/*
 * function:
 *    insert_udp_slice
 *
 * return:
 *    int 1 for error
 *
 * parameters:
 *    int sfd the socket fd
 *    struct sockaddr_storage *restrict storage the storage to send to
 *    uint8_t *restrict slice the buffer to insert
 *
 * notes:
 *    push the data into the udp packet
 *
 * */

int insert_udp_slice(
    const int sfd, struct sockaddr_storage *restrict storage, uint8_t *restrict slice) {
    static int8_t pcheckbuff[sizeof(struct pseudo_header) + sizeof(struct udphdr)];
    static int8_t buffer[sizeof(struct iphdr) + sizeof(struct udphdr)];

    static struct iphdr *iph   = (struct iphdr *)buffer;
    static struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct iphdr));
    static int ipid            = 54321;

    static const char *src_ip = "192.168.0.1";

    if (storage->ss_family == AF_INET6) {

        return 1;
    }
    struct sockaddr_in *sin = (struct sockaddr_in *)storage;

    struct pseudo_header *psh = (struct pseudo_header *)pcheckbuff;
    struct udphdr *udpch      = (struct udphdr *)(pcheckbuff + sizeof(struct pseudo_header));

    iph->ihl      = 5;
    iph->version  = 4;
    iph->tos      = 0;
    iph->tot_len  = sizeof(struct iphdr) + sizeof(struct udphdr);
    iph->id       = htonl(ipid++);
    iph->frag_off = 0;
    iph->ttl      = 255;
    iph->protocol = IPPROTO_UDP;
    iph->check    = 0; //Set to 0 before calculating checksum
    iph->saddr    = inet_addr(src_ip); //Spoof the source ip address
    iph->daddr    = sin->sin_addr.s_addr;

    iph->check = csum((uint16_t *)buffer, iph->tot_len);

    udph->source = htons(*(int16_t *)slice);
    udph->dest   = sin->sin_port; //already in net endianess
    udph->len    = htons(sizeof(struct udphdr));
    udph->check  = 0;

    psh->source_address = iph->saddr;
    psh->dest_address   = iph->daddr;
    psh->placeholder    = 0;
    psh->protocol       = IPPROTO_UDP;
    psh->udp_length     = htons(sizeof(struct udphdr));

    *udpch = *udph;

    udph->check
        = csum((uint16_t *)pcheckbuff, sizeof(struct pseudo_header) + sizeof(struct udphdr));


    ensure(
        sendto(sfd, buffer, iph->tot_len, 0, (struct sockaddr *)storage, sizeof(struct sockaddr_in))
        != -1);

    return 0;
}
