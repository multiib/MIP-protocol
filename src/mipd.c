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



void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, uint8_t *mip_addr);

int main(int argc, char *argv[]) {

    // VARIABLES
    struct epoll_event events[MAX_EVENTS];
    int raw_fd, listening_fd, epoll_fd; // File descriptors
    int rc; // Return code

    // To be set by CLI
    char *socket_upper;        // UNIX socket path
    uint8_t local_mip_addr;    // MIP Adress

    struct ping_data ping_data; // Struct for storing data from application
    struct forward_data forward_data; // Struct for storing data to be forwarded while waiting for ARP reply
    
    // uint8_t set_ttl = 15; //TODO: Implement user specified TTL


    uint8_t mip_return = 0; // Used to store MIP adresses while talking to ping_server
    uint8_t ttl_return;     // Used to store TTL while talking to ping_server

    int waiting_to_forward = 0; // Used to check if we are waiting for an ARP reply before forwarding a packet
    uint8_t waiting_next_hop_MIP; 


    uint8_t target_arp_mip_addr; // Used to store MIP address from SDU of ARP request

    struct ifs_data ifs; // Interface data

    int route_fd = -1; // File descriptor for routing daemon
    int app_fd   = -1; // File descriptor for application



    // PARSE ARGUMENTS FROM CLI
    parse_arguments(argc, argv, &debug_mode, &socket_upper, &local_mip_addr);

    printf("Debug mode: %d\n", debug_mode); //TODO: Remove
    printf("Socket upper: %s\n", socket_upper); //TODO: Remove
    printf("MIP address: %u\n", local_mip_addr); //TODO: Remove


    // SET UP NETWORKING UTILITIES
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

    // Initialize interface data
    init_ifs(&ifs, raw_fd, local_mip_addr);

    // Create UNIX listening socket for application traffic
    listening_fd = create_unix_sock(socket_upper);
    printf("Listening on %s\n", socket_upper); //TODO: Remove
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


    // MAIN LOOP FOR HANDLING TRAFFIC FROM APPLICATION AND MIP
    while(1) {

        // Wait for incoming events
        rc = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (rc == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        // Add new application connection to epoll instance
        if (events->data.fd == listening_fd) {
            
            // Accept new connection
            // We assume that the routing daemon always connects first for now
            printf("routing_fd: %d\n", route_fd);
            if (route_fd == -1){
                route_fd = accept(listening_fd, NULL, NULL);
                if (route_fd == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                printf("Routing daemon connected\n\n"); //TODO: Remove
                printf("Local MIP address: %u\n", local_mip_addr); //TODO: Remove



                rc = add_to_epoll_table(epoll_fd, route_fd);
                if (rc == -1) {
                    perror("add_to_epoll_table");
                    exit(EXIT_FAILURE);
                }

                // Send local MIP address to routing daemon
                rc = write(route_fd, &local_mip_addr, 1);
                if (rc == -1) {
                    perror("write");
                    exit(EXIT_FAILURE);
                }
                printf("Sent local MIP address to routing daemon\n\n"); //TODO: Remove

            } else {
                app_fd = accept(listening_fd, NULL, NULL);
                if (app_fd == -1) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                rc = add_to_epoll_table(epoll_fd, app_fd);
                if (rc == -1) {
                    perror("add_to_epoll_table");
                    exit(EXIT_FAILURE);
                }
            }


            if (debug_mode){
                printf("Application connected\n\n");
            }





        // INCOMING MIP TRAFFIC
        } else if (events->data.fd == raw_fd) {

            // Allocate memory for PDU struct
            struct pdu *pdu = alloc_pdu();

            // Index of recieving ethernet interface
            int recv_interface;

            // Handle incoming MIP packet and determine type of packet
            MIP_handle type = handle_mip_packet(raw_fd, &ifs, pdu, &recv_interface);

            // Check if packet is for another MIP daemon, forward if it is
            if (local_mip_addr != pdu->miphdr->dst){
                if (debug_mode){
                    printf("Packet not for us\n");
                }
                
                uint8_t next_hop_MIP = routing_lookup(pdu->miphdr->dst, &route_fd);

                // If next hop exists, forward packet
                if (next_hop_MIP != 255){
                    if (debug_mode){
                        printf("Forwarding packet to MIP: %u\n", next_hop_MIP);
                    }
                        // Get MAC address of next hop MIP
                        uint8_t *next_hop_MAC = arp_lookup(next_hop_MIP);

                        if (next_hop_MAC) {
                            // Known MAC address, forward the packet
                            uint8_t interface = arp_lookup_interface(next_hop_MIP);
                            send_mip_packet(&ifs, ifs.addr[interface].sll_addr, next_hop_MAC, ifs.local_mip_addr, next_hop_MIP, pdu->miphdr->ttl, pdu->miphdr->sdu_type, pdu->sdu, pdu->miphdr->sdu_len*sizeof(uint32_t));
                        } else {
                            // Unknown MAC address, send ARP request


                            fill_forward_data(&forward_data, next_hop_MIP, pdu, &waiting_to_forward, );
                            waiting_next_hop_MIP = next_hop_MIP;
                            send_arp_request_to_all_interfaces(&ifs, next_hop_MIP, debug_mode);
                        }

                } else if (debug_mode){
                    printf("No route to destination\n");
                }


                
            } else {
                if (debug_mode){
                    printf("Packet for us\n");
                }

                switch (type){

                    // RECIEVED MIP PING FROM OTHER MIP DAEMON
                    case MIP_PING:
                        if (debug_mode){
                            printf("\nReceived MIP_PING\n");
                            print_pdu_content(pdu);
                            printf("\n");
                        }

                        // Write SDU to ping_server
                        rc = write(app_fd, pdu->sdu, pdu->miphdr->sdu_len*sizeof(uint32_t));
                        if (rc == -1) {
                            perror("write");
                            exit(EXIT_FAILURE);
                        }
                        
                        // Store MIP address and TTL for return packet
                        mip_return = pdu->miphdr->src;
                        ttl_return = pdu->miphdr->ttl;

                        break;

                    // RECIEVED MIP PONG FROM OTHER MIP DAEMON
                    case MIP_PONG:
                        if (debug_mode){
                            printf("\nReceived MIP_PONG\n");
                            print_pdu_content(pdu);
                            printf("\n");
                        }

                        // Write SDU to ping_server
                        rc = write(app_fd, pdu->sdu, pdu->miphdr->sdu_len*sizeof(uint32_t));
                        if (rc == -1) {
                            perror("write");
                            exit(EXIT_FAILURE);
                        }


                        // We are done with the ping_client, close the connection
                        close(app_fd);

                        break;

                    // RECIEVED MIP ARP REQUEST FROM OTHER MIP DAEMON
                    case MIP_ARP_REQUEST:
                        if (debug_mode){
                            printf("\nReceived MIP_ARP_REQUEST\n");
                            print_pdu_content(pdu);
                            printf("\n");
                        }

                        // Get target MIP address from SDU of ARP request
                        decode_sdu_miparp(pdu->sdu, &target_arp_mip_addr);

                        // Check if ARP request is for this MIP daemon by comparing target MIP address with local MIP address






                        if (target_arp_mip_addr == ifs.local_mip_addr) {
                            if (debug_mode){
                                printf("ARP request for us\n");
                            }

                            // Create SDU for ARP reply containing matching MIP address
                            uint32_t *sdu = create_sdu_miparp(ARP_TYPE_REPLY, ifs.local_mip_addr);

                            // Update ARP table
                            arp_insert(pdu->miphdr->src, pdu->ethhdr->src_mac, recv_interface);

                            // Send ARP reply
                            if (debug_mode){
                                printf("Sending MIP_ARP_REPLY to MIP: %u\n", pdu->miphdr->src);
                            }

                            // Send ARP reply back to source
                            if (pdu->miphdr->ttl){
                                send_mip_packet(&ifs, ifs.addr[recv_interface].sll_addr, pdu->ethhdr->src_mac, ifs.local_mip_addr, pdu->miphdr->src, pdu->miphdr->ttl - 1, SDU_TYPE_MIPARP, sdu, 4);
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

                    // RECIEVED MIP ARP REPLY FROM OTHER MIP DAEMON
                    case MIP_ARP_REPLY:
                        if (debug_mode){
                            printf("\nReceived MIP_ARP_REPLY\n");
                            print_pdu_content(pdu);
                            printf("\n");
                        }

                        if (waiting_to_forward){

                            // Get MAC address of next hop MIP
                            uint8_t *next_hop_MAC = arp_lookup(waiting_next_hop_MIP);


                            send_mip_packet(&ifs, ifs.addr[recv_interface].sll_addr, next_hop_MAC, ifs.local_mip_addr, waiting_next_hop_MIP, forward_data.ttl, forward_data.sdu_type, forward_data.sdu, forward_data.sdu_len*sizeof(uint32_t));
                            clear_forward_data(&forward_data, &waiting_to_forward);
                        }





                        // Get target MIP address from SDU of ARP request
                        decode_sdu_miparp(pdu->sdu, &target_arp_mip_addr);

                        // Update ARP table
                        arp_insert(pdu->miphdr->src, pdu->ethhdr->src_mac, recv_interface);
                        
                        // Check if we are waiting for this reply by comparing target MIP address with local MIP address
                        if (ping_data.dst_mip_addr == target_arp_mip_addr){

                            // Create SDU 
                            uint8_t sdu_len;
                            uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);

                            // Get MAC address of destination MIP and what ethernet interface it is on
                            uint8_t *dst_mac_addr = arp_lookup(ping_data.dst_mip_addr);
                            uint8_t interface = arp_lookup_interface(ping_data.dst_mip_addr);
                            
                            if (debug_mode){
                                printf("Sending MIP_PING to MIP: %u\n", ping_data.dst_mip_addr);
                            }

                            // Send to destination MIP daemon
                            send_mip_packet(&ifs, ifs.addr[interface].sll_addr, dst_mac_addr, ifs.local_mip_addr, ping_data.dst_mip_addr, ping_data.ttl, SDU_TYPE_PING, sdu, sdu_len*sizeof(uint32_t));

                            free(sdu);
                            sdu = NULL;


                        } else if (debug_mode){
                            printf("Not waiting for this reply\n");
                        }

                        break;


                    // RECIEVED MIP ROUTE HELLO FROM OTHER MIP DAEMON
                    case MIP_ROUTE_HELLO:
                        if (debug_mode){
                            printf("\nReceived MIP_ROUTE_HELLO\n");
                            print_pdu_content(pdu);
                            printf("\n");
                        }

                        break;

                    // RECIEVED MIP ROUTE UPDATE FROM OTHER MIP DAEMON
                    case MIP_ROUTE_UPDATE:
                        if (debug_mode){
                            printf("\nReceived MIP_ROUTE_UPDATE\n");
                            print_pdu_content(pdu);
                            printf("\n");
                        }

                        break;


                    
                    // RECIEVED UNKNOWN MIP PACKET
                    default:
                        printf("Received unknown MIP packet\n");
                        break;
                }
                destroy_pdu(pdu);
            }
        // INCOMING APPLICATION TRAFFIC
        } else if (events->data.fd == app_fd){

            // Handle incoming application message and determine type of message
            APP_handle type = handle_app_message(events->data.fd, &ping_data.dst_mip_addr, ping_data.msg, &ping_data.ttl);

            switch (type){

                // RECIEVED MESSAGE FROM PING_CLIENT
                case APP_PING:
                    if (debug_mode){
                        printf("\nReceived APP_PING\n");
                        printf("Content: %s\n", ping_data.msg);
                    }
                    

                    // CHECK IF WE HAVE THE MAC ADDRESS FOR THE DESTINATION MIP ADDRESS
                    uint8_t * mac_addr = arp_lookup(ping_data.dst_mip_addr);

                    // If we have the MAC address, send the MIP packet
                    if (mac_addr) {
                        if (debug_mode){
                            printf("We have the MAC address for MIP %u\n", ping_data.dst_mip_addr);
                        }


                        // Create SDU 
                        uint8_t sdu_len;
                        uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);

                        // Get MAC address of destination MIP and what ethernet interface it is on
                        uint8_t *dst_mac_addr = arp_lookup(ping_data.dst_mip_addr);
                        uint8_t interface = arp_lookup_interface(ping_data.dst_mip_addr);
                        
                        if (debug_mode){
                            printf("Sending MIP_PING to MIP: %u\n", ping_data.dst_mip_addr);
                        }

                        // Send to ping_server
                        send_mip_packet(&ifs, ifs.addr[interface].sll_addr, dst_mac_addr, ifs.local_mip_addr, ping_data.dst_mip_addr, ping_data.ttl, SDU_TYPE_PING, sdu, sdu_len*sizeof(uint32_t));

                        free(sdu);
                        sdu = NULL;


                    // If we don't have the MAC address, send an ARP request
                    } else {
                        if (debug_mode){
                            printf("MAC address for MIP %u not found in cache\n", ping_data.dst_mip_addr);
                        }

                        // Create packet struct

                        send_arp_request_to_all_interfaces(&ifs, ping_data.dst_mip_addr, debug_mode);

                    }
                    break;


                // RECIEVED MESSAGE FROM PING_SERVER
                case APP_PONG:
                    if (debug_mode){
                        printf("Received APP_PONG\n");
                    }

                    // Create SDU
                    uint8_t sdu_len;
                    uint32_t *sdu = stringToUint32Array(ping_data.msg, &sdu_len);

                    // Get MAC address of destination MIP and what ethernet interface it is on
                    uint8_t *dst_mac_addr = arp_lookup(mip_return);
                    uint8_t interface = arp_lookup_interface(mip_return);

                    // Send MIP packet back to source
                    if (ttl_return){
                        send_mip_packet(&ifs, ifs.addr[interface].sll_addr, dst_mac_addr, ifs.local_mip_addr, mip_return, ttl_return-1, SDU_TYPE_PING, sdu, sdu_len*sizeof(uint32_t));
                    } else if (debug_mode){
                        printf("TTL = 0, dropping packet\n");
                    }

                    free(sdu);
                    sdu = NULL;

                    // Reset mip_return and ttl_return for next ping
                    mip_return = 0;
                    ttl_return = 0;

                    break;



                default:
                    if (debug_mode){
                        printf("Received unknown APP message\n");
                    }


                    
                    break;
            }
        } else if (events->data.fd == route_fd){
            printf("Received ROUTE\n");
            // print message

            uint8_t msg [8192];

            ROUTE_handle type = handle_route_message(route_fd, msg);

            switch (type)
            {
            case ROUTE_HELLO:
                printf("Received ROUTE_HELLO\n");
                break;

            case ROUTE_UPDATE:
                printf("Received ROUTE_UPDATE\n");
                break;

            case ROUTE_RESPONSE:
                printf("Received ROUTE_RESPONSE\n");
                break;
            
            default:
                printf("Received unknown ROUTE message\n");
                break;
            }

        } else {
            printf("Received unknown event\n");

        }
    }

    // Close listening socket
    close(raw_fd);


    return 0;
}


void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_upper, uint8_t *mip_addr) {
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

    *mip_addr = (uint8_t) mip_tmp;
}