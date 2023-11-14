#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

#include "arp.h"
#include "ether.h"
#include "pdu.h"
#include "utils.h"
#include "mip.h"
#include "ipc.h"



// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], int *debug_mode, char **usock_fd);

int main(int argc, char *argv[]) {
    int debug_mode = 0;
    char *usock_fd = NULL;

    parse_arguments(argc, argv, &debug_mode, &usock_fd);

    // Further processing with debug_mode and usock_fd
    // ...
}

// Definition of the parse_arguments function
void parse_arguments(int argc, char *argv[], int *debug_mode, char **usock_fd) {
    int opt;
    *debug_mode = 0; // Initialize debug_mode to 0 (off)

    while ((opt = getopt(argc, argv, "hd:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] [-d <usock_fd>]\n", argv[0]);
                exit(0);
            case 'd':
                *debug_mode = 1; // Set debug mode to on
                *usock_fd = optarg; // Set the file descriptor
                break;
            default:
                fprintf(stderr, "Usage: %s [-h] [-d <usock_fd>]\n", argv[0]);
                exit(1);
        }
    }

    if (!*debug_mode) {
        fprintf(stderr, "Debug mode (-d) is required.\n");
        exit(1);
    }
}
