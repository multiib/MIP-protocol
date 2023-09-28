#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include "common.h"



int main(int argc, char *argv[]) {
    int opt;
    int debug_mode = 0;
    char *socket_upper = NULL;
    char *mip_address = NULL;

    while ((opt = getopt(argc, argv, "dh")) != -1) {
        switch (opt) {
            case 'd':
                debug_mode = 1;
                break;
            case 'h':
                printf("Usage: %s [-h] [-d] <socket_upper> <MIP address>\n", argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Usage: %s [-h] [-d] <socket_upper> <MIP address>\n", argv[0]);
                exit(1);
        }
    }

    // After processing options, optind points to the first non-option argument
    if (optind + 2 != argc) {
        fprintf(stderr, "Usage: %s [-h] [-d] <socket_upper> <MIP address>\n", argv[0]);
        exit(1);
    }

    socket_upper = argv[optind];
    mip_address = argv[optind + 1];

    // Now you can use 'debug_mode', 'socket_upper', and 'mip_address' in your program

    // Example: Print the values
    printf("Debug mode: %s\n", debug_mode ? "enabled" : "disabled");
    printf("Socket upper: %s\n", socket_upper);
    printf("MIP address: %s\n", mip_address);

    return 0;
}
