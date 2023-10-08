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

void handle_upper_layer(); // For UNIX domain socket
void handle_lower_layer(); // For RAW socket
void mip_arp();            // Handle ARP logic

void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, char **mip_address);

int main(int argc, char *argv[]) {

    int debug_mode = 0;

    char *socket_upper = NULL;
    char *mip_address = NULL;

    struct ifs_data ifs;
    get_mac_from_interfaces(&so_addrs);
        // Loop to print the MAC addresses
    for (int i = 0; i < ifs.ifn; i++) {  // ifn would ideally contain the number of populated entries in addr[]
        print_mac_addr(ifs.addr[i].sll_addr, 6);  // The MAC address length is typically 6 bytes
    }

    // Call the function to parse arguments
    parse_arguments(argc, argv, &debug_mode, &socket_upper, &mip_address);

    // Create sockets
    // RAW socket and UNIX domain socket
    int raw_sock, rc;
    raw_sock = create_raw_socket();
    
    // Start Threads or select/poll loop for the below
    // handle_upper_layer();
    // handle_lower_layer();
    // mip_arp();

    // Now you can use 'debug_mode', 'socket_upper', and 'mip_address' in your program

    // // Example: Print the values
    // printf("Debug mode: %s\n", debug_mode ? "enabled" : "disabled");
    // printf("Socket upper: %s\n", socket_upper);
    // printf("MIP address: %s\n", mip_address);
    
    return 0;
}

// Definition of the parse_arguments function
void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, char **mip_address) {
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
    *mip_address = argv[optind + 1];
}

void handle_upper_layer() {
    // Handling logic for UNIX domain socket
}

void handle_lower_layer() {
    // Handling logic for RAW socket
}

void mip_arp() {
    // ARP logic here
}