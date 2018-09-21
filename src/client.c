#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "encoder.h"
#include "network.h"
#include "test.h"

int main(int argc, char** argv) {
    int port = 34854;
    int choice;
    while (1) {
        int option_index = 0;

#ifndef NDEBUG
        static struct option long_options[]
            = {{"version", no_argument, 0, 'v'}, {"test", no_argument, 0, 't'},
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
                break;
            case '?':
                /* getopt_long will have already printed an error */
                break;
            default:
                /* Not sure how to get here... */
                return EXIT_FAILURE;
        }
    }

    uint8_t slice[2];
    slice[0] = 0;
    slice[1] = 0;

    struct sockaddr_storage storage;
    make_storage(&storage, "127.0.0.1", port);

    insert_udp_slice(&storage, slice);

    return EXIT_SUCCESS;
}
