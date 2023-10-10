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


void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, uint8_t *mip_address);

int main(int argc, char *argv[]) {
    // Declaration of variables

    struct epoll_event ev, events[MAX_EVENTS]; // Epoll events
    int raw_fd, listening_fd, unix_fd, epoll_fd, rc;

    // To be set by CLI
    int debug_mode = 0;        // Debug flag
    char *socket_upper;        // UNIX socket path
    uint8_t mip_address;       // MIP Adress

    struct ifs_data ifs; // Struct to hold MAC addresses

    // Parse arguments
    parse_arguments(argc, argv, &debug_mode, &socket_upper, &mip_address);

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
            perror("epoll_create1");
            close(sd);
            exit(EXIT_FAILURE);
    }

    // Create RAW socket
    raw_fd = create_raw_socket();
    if (raw_fd == -1) {
        perror("create_raw_socket");
        exit(EXIT_FAILURE);
    }

    // Get MAC addresses from interfaces
    init_ifs(&ifs, raw_fd, mip_address);


    // Create UNIX listening socket
    listening_fd = create_unix_sock(socket_upper);



    // Add RAW socket to epoll instance
    rc = add_to_epoll_table(epoll_fd, &ev, raw_fd);
    if (rc == -1) {
        perror("add_to_epoll_table");
        exit(EXIT_FAILURE);
    }

    // Add UNIX listening socket to epoll instance
    rc = add_to_epoll_table(epoll_fd, &ev, listening_fd);
    if (rc == -1) {
        perror("add_to_epoll_table");
        exit(EXIT_FAILURE);
    }



	while(1) {
		rc = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (rc == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);

		} else if (events->data.fd == listening_fd) {

            unix_fd = accept(listening_fd, NULL, NULL);
            if (unix_fd == -1) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            rc = add_to_epoll_table(epoll_fd, &ev, unix_fd);
            if (rc == -1) {
                perror("add_to_epoll_table");
                exit(EXIT_FAILURE);
            }
		} else if (events->data.fd == raw_fd) {
            // Handle RAW socket
            //handle_raw_socket(raw_fd, &ifs, debug_mode);
            printf("RAW socket\n");
        } else {
            // Handle UNIX socket
            //handle_unix_socket(events->data.fd, &ifs, debug_mode);
            printf("UNIX socket\n");
            handle_client(events->data.fd);
        }


		break;
	}
	close(raw_fd);

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