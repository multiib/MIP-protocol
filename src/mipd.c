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


    struct epoll_event events[MAX_EVENTS]; // Epoll events
    int raw_fd, listening_fd, unix_fd, epoll_fd, rc;

    // To be set by CLI
    char *socket_upper;        // UNIX socket path
    uint8_t local_mip_addr;    // MIP Adress

    struct ping_data ping_data; // Ping data
    
    uint8_t set_ttl = 15;
    uint8_t set_ttl_broadcast = 1;

    uint8_t mip_return = 0;
    uint8_t ttl_return;

    int arp_type;
    uint8_t mip_addr;

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
    init_ifs(&ifs, raw_fd, local_mip_addr);

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
            if (debug_mode){
                printf("Application connected\n\n");
            }

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

            // Handle incoming MIP packet and determine type of packet
            MIP_handle type = handle_mip_packet(raw_fd, &ifs, pdu, &interface);

            switch (type){
                case MIP_PING:

                    if (debug_mode){
                        printf("\nReceived MIP_PING\n");
                        print_pdu_content(pdu);
                        printf("\n");
                    }
                    printf("0\n");
                    rc = write(unix_fd, pdu->sdu, pdu->miphdr->sdu_len);
                    if (rc == -1) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    printf("rc: %d\n", rc);
                    printf("1\n");
                    mip_return = pdu->miphdr->src;
                    printf("2\n");
                    ttl_return = pdu->miphdr->ttl;
                    printf("3\n");
                    break;

                case MIP_PONG:
                    if (debug_mode){
                        printf("\nReceived MIP_PONG\n");
                        print_pdu_content(pdu);
                        printf("\n");
                    }

                    rc = write(unix_fd, pdu->sdu, pdu->miphdr->sdu_len);
                    if (rc == -1) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }

                    mip_return = 0;

                    close(unix_fd); //

                    break;

                case MIP_ARP_REQUEST:
                    if (debug_mode){
                        printf("\nReceived MIP_ARP_REQUEST\n");
                        print_pdu_content(pdu);
                        printf("\n");
                    }

                    // Set type of MIP-ARP message and contained MIP address
                    decode_sdu_miparp(pdu->sdu, &arp_type, &mip_addr);

                    // Check if ARP request is for this MIP daemon by comparing target MIP address with local MIP address
                    if (mip_addr == ifs.local_mip_addr) {
                        if (debug_mode){
                            printf("ARP request for us\n");
                        }

                        // Create SDU for ARP reply containing matching MIP address
                        uint32_t *sdu = create_sdu_miparp(ARP_TYPE_REPLY, ifs.local_mip_addr);

                        // Update ARP table
                        arp_insert(pdu->miphdr->src, pdu->ethhdr->src_mac, interface);

                        // Send ARP reply
                        if (debug_mode){
                            printf("Sending MIP_ARP_REPLY to MIP: %u\n", pdu->miphdr->src);
                        }

                        if (pdu->miphdr->ttl){
                            send_mip_packet(&ifs, ifs.addr[interface].sll_addr, pdu->ethhdr->src_mac, ifs.local_mip_addr, pdu->miphdr->src, pdu->miphdr->ttl - 1, SDU_TYPE_MIPARP, sdu, 4);
                        } else {
                            if (debug_mode){
                                printf("TTL = 0, dropping packet\n");
                            }
                        }

                        // If ARP request is not for this MIP daemon, throw packet away
                    } else {
                        if (debug_mode){
                            printf("ARP request not for us\n");
                        }
                    }
                    break;


                case MIP_ARP_REPLY:
                    if (debug_mode){
                        printf("\nReceived MIP_ARP_REPLY\n");
                        print_pdu_content(pdu);
                        printf("\n");
                    }


                    // Set type of MIP-ARP message and contained MIP address
                    decode_sdu_miparp(pdu->sdu, &arp_type, &mip_addr);



                    // Update ARP table
                    arp_insert(pdu->miphdr->src, pdu->ethhdr->src_mac, interface);
                    
                    // CHECK IF WE ARE WAITING FOR THIS REPLY
                    if (ping_data.dst_mip_addr == mip_addr){

                        // Create SDU
                        uint8_t sdu_len;

                        uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);


                        uint8_t *dst_mac_addr = arp_lookup(ping_data.dst_mip_addr);
                        uint8_t interface = arp_lookup_interface(ping_data.dst_mip_addr);
                        
                        if (debug_mode){
                            printf("Sending MIP_PING to MIP: %u\n", ping_data.dst_mip_addr);
                        }


                        send_mip_packet(&ifs, ifs.addr[interface].sll_addr, dst_mac_addr, ifs.local_mip_addr, ping_data.dst_mip_addr, set_ttl, SDU_TYPE_PING, sdu, sdu_len);

                        free(sdu);
                        sdu = NULL;
                    } else if (debug_mode){
                        printf("Not waiting for this reply\n");
                    }

                    break;
                default:
                    printf("Received unknown MIP packet\n");
                    break;
            }
            printf("9\n");
            destroy_pdu(pdu);
            printf("10\n");

        } else {
            // If incoming application traffic

            // Type of application packet
            APP_handle type = handle_app_message(events->data.fd, &ping_data.dst_mip_addr, ping_data.msg);

            switch (type){
                case APP_PING:
                    if (debug_mode){
                        printf("\nReceived APP_PING\n");
                        printf("Content: %s\n", ping_data.msg);
                    }
                    

                    // Check if we have the MAC address of the destination MIP
                    uint8_t * mac_addr = arp_lookup(ping_data.dst_mip_addr);
                    if (mac_addr) {
                        if (debug_mode){
                            printf("We have the MAC address for MIP %u\n", ping_data.dst_mip_addr);
                        }


                        // Create SDU
                        uint8_t sdu_len;

                        uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);


                        uint8_t *dst_mac_addr = arp_lookup(ping_data.dst_mip_addr);
                        uint8_t interface = arp_lookup_interface(ping_data.dst_mip_addr);
                        
                        if (debug_mode){
                            printf("Sending MIP_PING to MIP: %u\n", ping_data.dst_mip_addr);
                        }

                        // Send to ping_server
                        send_mip_packet(&ifs, ifs.addr[interface].sll_addr, dst_mac_addr, ifs.local_mip_addr, ping_data.dst_mip_addr, set_ttl, SDU_TYPE_PING, sdu, sdu_len);

                        free(sdu);
                        sdu = NULL;


                    } else {
                        if (debug_mode){
                            printf("MAC address for MIP %u not found in cache\n", ping_data.dst_mip_addr);
                        }
                        // SEND ARP REQUEST

                        // Create SDU
                        uint32_t *sdu = create_sdu_miparp(ARP_TYPE_REQUEST, ping_data.dst_mip_addr);

                        // Create Broadcast MAC address
                        uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

                        // Create broadcast MIP address
                        uint8_t broadcast_mip_addr = 0xff;

                        // Create SDU length
                        uint8_t sdu_len = 4;
                        

                        for (int interface = 0; interface < ifs.ifn; interface++) {
                            if (debug_mode){
                                printf("Sending MIP_BROADCAST to MIP: %u\n", broadcast_mip_addr);
                            }
                            send_mip_packet(&ifs, ifs.addr[interface].sll_addr, broadcast_mac, ifs.local_mip_addr, broadcast_mip_addr, set_ttl_broadcast, SDU_TYPE_MIPARP, sdu, sdu_len);
                        }

                    }
                    break;

                case APP_PONG:
                    if (debug_mode){
                        printf("Received APP_PONG\n");
                    }
                    uint8_t sdu_len;

                    uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);

                    uint8_t *dst_mac_addr = arp_lookup(mip_return);
                    uint8_t interface = arp_lookup_interface(mip_return);

                    // Send MIP packet back to source
                    if (ttl_return){
                        send_mip_packet(&ifs, ifs.addr[interface].sll_addr, dst_mac_addr, ifs.local_mip_addr, mip_return, ttl_return-1, SDU_TYPE_PING, sdu, sdu_len);
                    } else if (debug_mode){
                        printf("TTL = 0, dropping packet\n");
                    }
                    free(sdu);
                    sdu = NULL;
                    break;
                default:
                    if (debug_mode){
                        printf("Received unknown APP message\n");
                    }
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