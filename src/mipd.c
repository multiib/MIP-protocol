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


void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, uint8_t **mip_address);

int main(int argc, char *argv[]) {
    // Declaration of variables
    int rc;                    // Return code
    struct epoll_event ev, events[MAX_EVENTS]; // Epoll events

    // To be set by CLI
    int debug_mode = 0;        // Debug flag
    char *socket_upper = NULL; // UNIX socket path
    uint8_t mip_address;       // MIP Adress

    struct ifs_data ifs; // Struct to hold MAC addresses, corresponding raw sockets and number of interfaces

    // Parse arguments
    parse_arguments(argc, argv, &debug_mode, &socket_upper, &mip_address);

    // Create UNIX socket TODO: Create function for this
    int unix_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (unix_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Call the function to get MAC addresses and set up raw sockets
    init_ifs(&ifs);

    // Create epoll table
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}
    

    return 0;
}


void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, uint8_t *mip_address) {
    int opt;
    while ((opt = getopt(argc, argv, "dh")) != -1) {
        switch (opt) {
            case 'd':
                *debug_mode = 1;
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

    *socket_upper = argv[optind];

    // Convert MIP address from string to uint8_t and check for errors
    int mip_tmp = atoi(argv[optind + 1]);
    if (mip_tmp < 0 || mip_tmp > 255) {
        fprintf(stderr, "Invalid MIP address. Must be between 0 and 254.\n");
        exit(1);
    }
    if (mip_tmp == 255) {
        fprintf(stderr, "Reserved MIP address. Must be between 0 and 254.\n");
        exit(1);
    }

    *mip_address = (uint8_t) mip_tmp;
}