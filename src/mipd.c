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



void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, uint8_t *local_mip_addr);

int main(int argc, char *argv[]) {

    printf("Starting MIP daemon...\n");

    struct epoll_event events[MAX_EVENTS]; // Epoll events
    int raw_fd, listening_fd, unix_fd, epoll_fd, rc, send_ping_on_arp_reply = 0;

    // To be set by CLI
    int debug_mode = 0;        // Debug flag
    char *socket_upper;        // UNIX socket path
    uint8_t local_mip_addr;    // MIP Adress

    struct ping_data ping_data; // Ping data

    // Deamon network data
    struct ifs_data ifs;

    // Parse arguments from CLI
    parse_arguments(argc, argv, &debug_mode, &socket_upper, &local_mip_addr);

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
    }

    // Create RAW socket for MIP traffic
    raw_fd = create_raw_socket();
    if (raw_fd == -1) {
        perror("create_raw_socket");
        exit(EXIT_FAILURE);
    }

    // Initialize network data
    //print local mip addr
    printf("Local MIP addr: %d\n", local_mip_addr);
    init_ifs(&ifs, raw_fd, local_mip_addr);
    printf("Local MIP addr: %d\n", ifs.local_mip_addr);


    // Create UNIX listening socket for accepting connections from applications
    listening_fd = create_unix_sock(socket_upper);

    // Add RAW socket to epoll instance
    rc = add_to_epoll_table(epoll_fd, raw_fd);
    if (rc == -1) {
        perror("add_to_epoll_table");
        exit(EXIT_FAILURE);
    }

    // Add UNIX listening socket to epoll instance
    rc = add_to_epoll_table(epoll_fd, listening_fd);
    if (rc == -1) {
        perror("add_to_epoll_table");
        exit(EXIT_FAILURE);
    }



    while(1) {
        // Wait for incoming events
        rc = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (rc == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);


        }
        // Add new application connection to epoll instance
        if (events->data.fd == listening_fd) {

            unix_fd = accept(listening_fd, NULL, NULL);
            if (unix_fd == -1) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("Application connected\n");

            rc = add_to_epoll_table(epoll_fd, unix_fd);
            if (rc == -1) {
                perror("add_to_epoll_table");
                exit(EXIT_FAILURE);
            }


        // If incoming MIP traffic
        } else if (events->data.fd == raw_fd) {
            // Data to be read from RAW socket
            struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));    

            // Index of recieving interface
            int interface;

            // Type of MIP packet
            MIP_handle type = handle_mip_packet(raw_fd, &ifs, pdu, &interface);

            switch (type){
                case MIP_PING:
                    printf("Received PING\n");
                    // SEND PING TO APP (ping_server)

                    break;

                case MIP_PONG:
                    printf("Received PONG\n");
                    // SEND PONG TO APP (ping_client)

                    break;

                case MIP_ARP_REQUEST:
                    printf("Received ARP request\n");

                    int arp_type;
                    uint8_t mip_addr;
                    
                    // Reverse sdu_array
                    decode_sdu_miparp(pdu->sdu, &arp_type, &mip_addr);




                    // CHECK IF THE REQUEST IS FOR US
                    uint32_t shiftedValue = pdu->sdu[0] >> 16;

                    // Mask the least-significant 8 bits
                    uint8_t isolatedValue = shiftedValue & 0xFF;



                    if (mip_addr == ifs.local_mip_addr) {

                        // IF YES, SEND ARP REPLY
                        printf("ARP request for us\n");
                        // Create SDU
                        uint32_t *sdu = create_sdu_miparp(ARP_TYPE_REPLY, pdu->miphdr->src);

                        // Update ARP table
                        arp_insert(pdu->miphdr->src, pdu->ethhdr->src_mac, interface);


                        // Send MIP packet

                        
                        send_mip_packet(&ifs, ifs.addr[interface].sll_addr, pdu->ethhdr->src_mac, pdu->miphdr->src, pdu->miphdr->src, 1, SDU_TYPE_MIPARP, sdu, 4);
                        // IF NO, IGNORE
                    } else {
                        printf("ARP request not for us\n");
                    }
                    break;

                case MIP_ARP_REPLY:
                    printf("Received ARP reply\n");

                    // Update ARP table
                    arp_insert(pdu->miphdr->src, pdu->ethhdr->src_mac, interface);
                    // CHECK IF WE ARE WAITING FOR THIS REPLY
                    if (send_ping_on_arp_reply){

                        // Create SDU
                        uint8_t sdu_len;
                        // Creat arr
                        
                        uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);

                        uint8_t *dst_mac_adr = arp_lookup(ping_data.dst_mip_addr);
                        
                        printf("ANSJOS\n");
                        send_mip_packet(&ifs, dst_mac_adr, pdu->ethhdr->src_mac, ifs.local_mip_addr, ping_data.dst_mip_addr, pdu->miphdr->ttl-1, SDU_TYPE_PING, sdu, sdu_len);

                        send_ping_on_arp_reply = 0;
                    }
                        // IF YES, SEND PING

                        // IF NO, IGNORE
                    break;
                default:
                    printf("Received unknown MIP packet\n");
                    break;
            }
            

            destroy_pdu(pdu);

        } else {
            // If incoming application traffic

            // Data to be read from application
            
            

            // Type of application packet
            APP_handle type = handle_app_message(events->data.fd, &ping_data.dst_mip_addr, ping_data.msg);

            switch (type){
                case APP_PING:
                    printf("Received APP_PING\n");

                    // Check if we have the MAC address of the destination MIP

                    // Check if we have the MAC address of the destination MIP
                    uint8_t * mac_addr = arp_lookup(ping_data.dst_mip_addr);
                    if (mac_addr) {
                        printf("We have the MAC address for MIP %u\n", ping_data.dst_mip_addr);
                        // SEND MIP PING

                        // send_mip_packet(&ifs, ifs.addr[interface].sll_addr, broadcast_mac, broadcast_mip_addr, dst_mip_addr, 1, SDU_TYPE_MIPARP, sdu);




                    } else {
                        printf("MAC address for MIP %u not found in cache\n", ping_data.dst_mip_addr);
                        // SEND ARP REQUEST

                        // Create SDU


                        uint32_t *sdu = create_sdu_miparp(ARP_TYPE_REQUEST, ping_data.dst_mip_addr);

                        // Print int
                        printf("SDU int: %u\n", sdu[0]);


                        // Create Broadcast MAC address
                        uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

                        // Create broadcast MIP address
                        uint8_t broadcast_mip_addr = 0xff;

                        // Create SDU length
                        uint8_t sdu_len = 4;
                        

                        for (int interface = 0; interface < ifs.ifn; interface++) {
                            send_mip_packet(&ifs, ifs.addr[interface].sll_addr, broadcast_mac, ifs.local_mip_addr, broadcast_mip_addr, 1, SDU_TYPE_MIPARP, sdu, sdu_len);
                        }

                        send_ping_on_arp_reply = 1;
                    }
                    break;

                case APP_PONG:
                    printf("Received APP_PONG\n");

                    // Send MIP PONG

                    break;
                
                default:
                    printf("Received unknown APP message\n");
                    break;
            }












            

        }
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