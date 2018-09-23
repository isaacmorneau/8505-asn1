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

int add_epoll_fd(int efd, int ifd) {
    int ret;
    static struct epoll_event event;
    event.data.fd = ifd;
    event.events  = EPOLLIN | EPOLLEXCLUSIVE;
    ensure((ret = epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event)) != -1);
    return ret;
}

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
    *wslice          = addr->sin_port;

    return 0;
}

int insert_udp_slice(
    const int sfd, struct sockaddr_storage *restrict storage, uint8_t *restrict slice) {
    static int8_t pcheckbuff[sizeof(struct pseudo_header) + sizeof(struct udphdr)];
    static int8_t buffer[sizeof(struct iphdr) + sizeof(struct udphdr)];

    static struct iphdr *iph   = (struct iphdr *)buffer;
    static struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct iphdr));
    static int ipid            = 54321;

    static const char *src_ip = "192.168.0.1";

    if (storage->ss_family == AF_INET6) {
        //TODO support IPv6
        return 1;
    }
    struct sockaddr_in *sin = (struct sockaddr_in *)storage;

    struct pseudo_header *psh = (struct pseudo_header *)pcheckbuff;
    struct udphdr *udpch      = (struct udphdr *)(pcheckbuff + sizeof(struct pseudo_header));

    iph->ihl      = 5;
    iph->version  = 4;
    iph->tos      = 0;
    iph->tot_len  = sizeof(struct iphdr) + sizeof(struct udphdr);
    iph->id       = htonl(ipid++); //TODO mimic regular stack incrementing numbers better
    iph->frag_off = 0;
    iph->ttl      = 255;
    iph->protocol = IPPROTO_UDP;
    iph->check    = 0; //Set to 0 before calculating checksum
    iph->saddr    = inet_addr(src_ip); //Spoof the source ip address
    iph->daddr    = sin->sin_addr.s_addr;

    iph->check = csum((uint16_t *)buffer, iph->tot_len);

    udph->source = *(int16_t *)slice;
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

    //TODO support IPv6
    ensure(
        sendto(sfd, buffer, iph->tot_len, 0, (struct sockaddr *)storage, sizeof(struct sockaddr_in))
        != -1);

    return 0;
}
