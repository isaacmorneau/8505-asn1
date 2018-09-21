#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "network.h"
#include "test.h"

void* server_handler(void* vport) {
    int efd = make_epoll();
    int sfd = make_bound_udp(*(int*)vport);
    set_non_blocking(sfd);

    struct epoll_event* events = make_epoll_events();

    add_epoll_fd(efd, sfd);

    while (1) {
        static int n, i, infd;
        n = wait_epoll(efd, events);
        for (i = 0; i < n; i++) {
            if (EVENT_ERR(events, i) || EVENT_HUP(events, i)) {
                // a socket got closed
                continue;
            } else if (EVENT_IN(events, i)) {
                infd = EVENT_FD(events, i);
                static uint8_t slice[2];
                static struct sockaddr_storage storage;
                extract_udp_slice(infd, &storage, slice);
                printf(">%02X %02X\n", slice[0], slice[1]);
                //TODO respond with ICMP
                //TODO add slice to encoder fragment
            }
        }
    }

    free(events);
}

void start_listening(const int port) {
    pthread_t th_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&th_id, &attr, server_handler, (void*)&port);
    //pthread_detach(th_id);
    void* ret = 0;
    pthread_join(th_id, ret);
}

int main(int argc, char** argv) {
    int port = 34854;
    int choice;
    while (1) {
        int option_index = 0;
#ifndef NDEBUG
        static struct option long_options[]
            = {{"version", no_argument, 0, 'v'}, {"tests", no_argument, 0, 't'},
                {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};
        choice = getopt_long(argc, argv, "vhtp:", long_options, &option_index);
#else
        static struct option long_options[] = {{"version", no_argument, 0, 'v'},
            {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};
        choice = getopt_long(argc, argv, "vhp:", long_options, &option_index);
#endif

        if (choice == -1)
            break;
        switch (choice) {
            case 'v':
                puts("v0.1");
                break;
            case 'p':
                port = atoi(optarg);
                break;
#ifndef NDEBUG
            case 't':
                run_encoders_tests();
                return EXIT_SUCCESS;
#endif
            case 'h':
                puts("read the source");
                return EXIT_SUCCESS;
            case '?':
                /* getopt_long will have already printed an error */
                break;
            default:
                /* Not sure how to get here... */
                return EXIT_FAILURE;
        }
    }

    start_listening(port);

    return EXIT_SUCCESS;
}
