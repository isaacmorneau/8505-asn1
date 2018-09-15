#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int choice;
    while (1) {
        static struct option long_options[]
            = {{"version", no_argument, 0, 'v'}, {"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

        int option_index = 0;

        choice = getopt_long(argc, argv, "vh", long_options, &option_index);

        if (choice == -1)
            break;
        switch (choice) {
            case 'v':
                puts("v0.1");
                break;
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

    return EXIT_SUCCESS;
}
