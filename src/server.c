#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <pthread.h>
#include "test.h"
#include "server.h"
#include "network.h"

int main(int argc, char** argv) {
    const char* port = "38499";
    int choice;
    while (1) {
        static struct option long_options[]
            = {{"version", no_argument, 0, 'v'}, {"tests", no_argument, 0, 't'},{"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

        int option_index = 0;

        choice = getopt_long(argc, argv, "vht", long_options, &option_index);

        if (choice == -1)
            break;
        switch (choice) {
            case 'v':
                puts("v0.1");
                break;
            case 't':
                run_encoders_tests();
                return EXIT_SUCCESS;
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

void* server_handler(void* vport) {
    int sfd = make_bound_udp(*(int*)vport);
    set_non_blocking(sfd);
    int efd = make_epoll();
    epoll_event* events = make_epoll_events();

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
                //
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
    pthread_detach(th_id);
}
