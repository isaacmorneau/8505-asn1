#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "encoder.h"
#include "network.h"
#include "test.h"

void print_help() {
    puts("-[-v]ersion - current version\n"
         "-[-t]est    - run the test suite if compiled in debug mode\n"
         "-[-f]ile    - the path to the file to recv\n"
         "-[-p]ort    - the server port to connect to\n"
         "--help      - this message\n");
}

const char* path = 0;

void* server_handler(void* vport) {
    encoder_frame_t enc;
    inbound_encoder_init(&enc, 0, 2);

    FILE* fp = fopen(path, "w+");
    if (!fp) {
        perror("error opening file");
    }

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
                //TODO respond with ICMP
                encoder_add_next(&enc, slice);
                if (encoder_finished(&enc)) {
                    fwrite(encoder_data(&enc), 1, enc.len, fp);
                    fflush(fp);
                    encoder_print(&enc);
                    encoder_close(&enc);
                    inbound_encoder_init(&enc, 0, 2);
                }
            }
        }
    }

    encoder_close(&enc);
    fclose(fp);
    free(events);
}

int main(int argc, char** argv) {
    int port = 34854;
    int choice;
    while (1) {
        int option_index = 0;
#ifndef NDEBUG
#define OPTSTR "vtp:f:h"
        static struct option long_options[] = {{"version", no_argument, 0, 'v'},
            {"tests", no_argument, 0, 't'}, {"file", required_argument, 0, 'f'},
            {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};
#else
#define OPTSTR "vp:f:h"
        static struct option long_options[]
            = {{"version", no_argument, 0, 'v'}, {"file", required_argument, 0, 'f'},
                {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};
#endif

        choice = getopt_long(argc, argv, OPTSTR, long_options, &option_index);
        if (choice == -1)
            break;
        switch (choice) {
            case 'v':
                puts("v0.1");
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                path = optarg;
                break;
#ifndef NDEBUG
            case 't':
                run_encoders_tests();
                return EXIT_SUCCESS;
#endif
            case 'h':
            case '?':
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    if (!path || !port) {
        print_help();
        return EXIT_FAILURE;
    }

    pthread_t th_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&th_id, &attr, server_handler, (void*)&port);
    //pthread_detach(th_id);
    void* ret = 0;
    pthread_join(th_id, ret);

    return EXIT_SUCCESS;
}
