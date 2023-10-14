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
    // Declaration of variables
    printf("Starting MIP daemon...\n");
    struct epoll_event events[MAX_EVENTS]; // Epoll events
    int raw_fd, listening_fd, unix_fd, epoll_fd, rc;

    // To be set by CLI
    int debug_mode = 0;        // Debug flag
    char *socket_upper;        // UNIX socket path
    uint8_t local_mip_addr;       // MIP Adress

    struct ifs_data ifs; // Struct to hold MAC addresses

    // Parse arguments
    parse_arguments(argc, argv, &debug_mode, &socket_upper, &local_mip_addr);

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
    }

    // Create RAW socket
    raw_fd = create_raw_socket();
    if (raw_fd == -1) {
        perror("create_raw_socket");
        exit(EXIT_FAILURE);
    }

    // Get MAC addresses from interfaces
    init_ifs(&ifs, raw_fd, local_mip_addr);


    // Create UNIX listening socket
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
        // Wait for events
        rc = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (rc == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);


        }
        // If event is from listening socket
        if (events->data.fd == listening_fd) {

            unix_fd = accept(listening_fd, NULL, NULL);

            if (unix_fd == -1) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("New connection\n");

            rc = add_to_epoll_table(epoll_fd, unix_fd);
            if (rc == -1) {
                perror("add_to_epoll_table");
                exit(EXIT_FAILURE);
            }
            printf("Added to epoll table\n");





        // If event is from RAW socket
        } else if (events->data.fd == raw_fd) {

            MIP_handle type = handle_mip_packet(raw_fd, &ifs);
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
                    // CHECK IF THE REQUEST IS FOR US

                        // IF YES, SEND ARP REPLY

                        // IF NO, IGNORE
                    break;

                case MIP_ARP_REPLY:
                    printf("Received ARP reply\n");
                    // CHECK IF WE ARE WAITING FOR THIS REPLY

                        // IF YES, SEND PING

                        // IF NO, IGNORE
                    break;
                default:
                    printf("Received unknown MIP packet\n");
                    break;
            }

        } else {
            // If event is from application

            // Data to be read from application
            uint8_t dst_mip_addr;
            char msg[256];


            APP_handle type = handle_app_message(events->data.fd, &dst_mip_addr, msg);
            switch (type){
                case APP_PING:
                    printf("Received APP_PING\n");

                    // Check if we have the MAC address of the destination MIP

                    // Check if we have the MAC address of the destination MIP
                    uint8_t * mac_addr = arp_lookup(dst_mip_addr);
                    if (mac_addr) {
                        printf("We have the MAC address for MIP %u\n", dst_mip_addr);
                        // SEND MIP PING

                        // send_mip_packet(&ifs, ifs.addr[interface].sll_addr, broadcast_mac, broadcast_mip_addr, dst_mip_addr, 1, SDU_TYPE_MIPARP, sdu);




                    } else {
                        printf("MAC address for MIP %u not found in cache\n", dst_mip_addr);
                        // SEND ARP REQUEST

                        // Create SDU


                        const char* sdu = create_sdu_miparp(ARP_TYPE_REQUEST, dst_mip_addr);

                        // Print SDU
                        printf("SDU: ");
                        for (int i = 0; i < 8; i++) {
                            printf("%02x ", sdu[i]);
                        }

                        // Create Broadcast MAC address
                        uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

                        // Create broadcast MIP address
                        uint8_t broadcast_mip_addr = 0xff;


                        for (int interface = 0; interface < ifs.ifn; interface++) {
                            send_mip_packet(&ifs, ifs.addr[interface].sll_addr, broadcast_mac, broadcast_mip_addr, dst_mip_addr, 1, SDU_TYPE_MIPARP, sdu);
                        }
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












            
            // //OLD CODE
            // // If this we have not read the message from application
            // if (new_app){
            //     int app_type, rc;

            //     new_app = 0; 
                
            //     // Buffer to hold message from application
            //     char buf[256];

            //     // Clear buffer
            //     memset(buf, 0, sizeof(buf));

            //     // Read message from application
            //     rc = read(events->data.fd, buf, sizeof(buf));
            //     if (rc <= 0) {
            //         perror("read");
            //         exit(EXIT_FAILURE);
            //     }

            //     // Set the destination_mip to the first byte of the buffer
            //     dst_mip_addr = (uint8_t) buf[0];

            //     // Initialize an offset for the message
            //     int offset = 1; // Skip the first byte (destination_mip)

            //     // Set app_type
            //     if (strncmp(buf + offset, "PING:", 5) == 0) {
            //         app_type = CLIENT;
            //         offset += 5;
            //     } else if (strncmp(buf + offset, "PONG:", 5) == 0) {
            //         app_type = SERVER;
            //         offset += 5;
            //     } else {
            //         perror("Unknown message type");
            //         close(fd);
            //         exit(EXIT_FAILURE);
            //     }
            //     // Copy the rest of the buffer to msg
            //     strcpy(msg, buf + offset);
            // }




            // if (app_type == CLIENT) {
            //     struct sockaddr_ll *dst_mac_entry = arp_lookup_mac(dst_mip_addr);
                
            //     if (dst_mac_entry == NULL) {
            //         // Send ARP request
            //         uint32_t sdu = create_sdu_miparp(SDU_ARP_TYPE_LOOKUP, dst_mip_addr);

            //         if (!waiting_for_arp_reply) {
            //             // Iterate over all interfaces and send ARP request
            //             for (int i = 0; i < ifs.num_ifs; i++){
            //                 send_mip_packet(&ifs, &ifs.addr[i].sll_addr, ARP_BROADCAST, ifs.local_mip_addr, dst_mip_addr, (const char*)&sdu, 0x01);
            //             }
            //         }
            //         waiting_for_arp_reply = 1;
            //     } else {
            //         // Send PING
            //         struct sockaddr_ll *src_mac_entry = arp_lookup_interface(dst_mip_addr);
            //         if (src_mac_entry) {
            //             send_mip_packet(&ifs, src_mac_entry->sll_addr, dst_mac_entry->sll_addr, ifs.local_mip_addr, dst_mip_addr, msg, 0x00);
            //         } else {
            //             perror("Could not find interface");
            //             exit(EXIT_FAILURE);
            //         }
            //     }
            // }




            // if (app_type == SERVER) {
            //     // Print the message
            //     printf("Received PONG: %s\n", msg);
            // }
            // else {
            //     perror("Unknown message type");

            //     exit(EXIT_FAILURE);
            // }



            // // Check ARP cache for MAC address matching the destination IP
            // // uint8_t *mac = arp_lookup(destination_mip);
            
            // // if (mac == NULL) {
            // //     // MAC address not in ARP cache, send ARP request
            // //     send_raw_packet(raw_fd, ARP_BROADCAST, NULL);
            // //     // In the next iteration, the ARP reply will be processed when events->data.fd == raw_fd
            // // } else {
            // //     // MAC address found in ARP cache, forward the ping message
            // //     send_raw_packet(raw_fd, mac, msg);
            // // }




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