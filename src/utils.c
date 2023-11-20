#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <errno.h>


#include "utils.h"
#include "arp.h"
#include "ether.h"
#include "pdu.h"
#include "mip.h"
#include "route.h"

#define REQUEST_MSG_LEN 6
#define RESPONSE_MSG_LEN 6

int debug_mode = 0;




/**
 * Print MAC address in hexadecimal format
 * addr: pointer to the MAC address array.
 * len: length of the MAC address array.
 * 
 * The function iterates through the MAC address array and prints
 * each byte in hexadecimal format, separated by a colon.
 */

void print_mac_addr(uint8_t *addr, size_t len)
{
    size_t i;

    for (i = 0; i < len - 1; i++) {
        printf("%02x:", addr[i]);
    }
    printf("%02x\n", addr[i]);
}



// Prepare a raw socket for receiving and sending MIP packets
int create_raw_socket(void)
{
    int sd;
    short unsigned int protocol = ETH_P_MIP;

    /* Set up a raw AF_PACKET socket without ethertype filtering */
    sd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
    if (sd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    return sd;
}

/*
 * This function stores struct sockaddr_ll addresses for all interfaces of the
 * node (except loopback interface)
 */
void get_mac_from_ifaces(struct ifs_data *ifs)
{
        struct ifaddrs *ifaces, *ifp;
        int i = 0;

        /* Enumerate interfaces: */
        /* Note in man getifaddrs that this function dynamically allocates
           memory. It becomes our responsability to free it! */
        if (getifaddrs(&ifaces)) {
                perror("getifaddrs");
                exit(-1);
        }

        /* Walk the list looking for ifaces interesting to us */
        for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
                /* We make sure that the ifa_addr member is actually set: */
                if (ifp->ifa_addr != NULL &&
                    ifp->ifa_addr->sa_family == AF_PACKET &&
                    strcmp("lo", ifp->ifa_name))
            /* Copy the address info into the array of our struct */
                        memcpy(&(ifs->addr[i++]),
                               (struct sockaddr_ll*)ifp->ifa_addr,
                               sizeof(struct sockaddr_ll));
        }
        /* After the for loop, the address info of all interfaces are stored */
        /* Update the counter of the interfaces */
        ifs->ifn = i;

        /* Free the interface list */
        freeifaddrs(ifaces);
}

// Initialize the interface data structure
void init_ifs(struct ifs_data *ifs, int rsock, uint8_t mip_addr)
{
    /* Get some info about the local ifaces */
    get_mac_from_ifaces(ifs);

    /* We use one RAW socket per node */
    ifs->rsock = rsock;

    /* Set the local MIP address */
    ifs->local_mip_addr = mip_addr;
}

// Create a MIP ARP SDU
uint32_t* create_sdu_miparp(int arp_type, uint8_t mip_addr) {
    uint32_t *sdu_array = (uint32_t*) malloc(sizeof(uint32_t));
    if (sdu_array == NULL) {
        return NULL; // Failed to allocate memory
    }

    uint32_t sdu = 0;

    if (arp_type) {
        sdu |= (1 << 31);
    }

    sdu |= (mip_addr << 23);

    sdu_array[0] = sdu;

    return sdu_array;
}




// Fill a buffer with a MIP ARP SDU
void fill_ping_buf(char *buf, size_t buf_size, const char *destination_host, const char *message, const char *ttl) {
    // Initialize the buffer to zeros


    memset(buf, 0, buf_size);


    buf[0] = atoi(destination_host);

    buf[1] = atoi(ttl);

    // Add "PING:"
    strcat(buf, "PING:");

    // Then add message
    if (message != NULL) {
        strcat(buf, message);
    }

}

void fill_pong_buf(char *buf, size_t buf_size, const char *destination_host, const char *message) {
    // Initialize the buffer to zeros
    memset(buf, 0, buf_size);

    buf[0] = atoi(destination_host);
    buf[1] = 0x00; // Filler value

    // Add "PING:"
    strcat(buf, "PONG:");

    // Then add message
    if (message != NULL) {
        strcat(buf, message);
    }
}

 /**
 * Handle received MIP packets and determine their type.
 *
 * raw_fd: File descriptor for the raw socket.
 * ifs: Pointer to the interface data structure.
 * pdu: Pointer to the protocol data unit structure.
 * recv_ifs_index: Pointer to store the index of the receiving interface.
 * 
 * The function first checks if the pdu is not NULL. It then receives 
 * the serialized buffer from the raw socket and deserializes it into the PDU structure.
 * Based on the type of SDU present in the MIP header, the function determines 
 * if the received packet is of type MIP_ARP_REQUEST, MIP_ARP_REPLY, MIP_PING, or MIP_PONG.
 * 
 * If in debug mode, the function will print additional details about the received PDU.
 * 
 * Returns the type of the received MIP packet. If there's an error or an unknown type,
 * the function returns -1.
 */

MIP_handle handle_mip_packet(struct ifs_data *ifs, struct pdu *pdu, int *recv_ifs_index)
{
    // Make sure pdu is not NULL
    if (pdu == NULL) {
        perror("NULL pdu argument");
        return -EINVAL;
    }

    MIP_handle mip_type;

    uint32_t rcv_buf[512];
    struct sockaddr_ll from_addr;
    socklen_t from_addr_len = sizeof(from_addr);
    
    /* Recv the serialized buffer via RAW socket */
    if (recvfrom(ifs->rsock, rcv_buf, 512, 0, (struct sockaddr *)&from_addr, &from_addr_len) <= 0) {
        perror("recvfrom()");
        close(ifs->rsock);
        return -1;
    }

    *recv_ifs_index = find_matching_if_index(ifs, &from_addr);

    size_t rcv_len = mip_deserialize_pdu(pdu, rcv_buf);

    printf("Received PDU with content (size %zu):\n", rcv_len);
    print_pdu_content(pdu);





    if (pdu->miphdr->sdu_type == SDU_TYPE_MIPARP) {
        int arp_type = (pdu->sdu[0] >> 31) & 1;

        if (arp_type == ARP_TYPE_REQUEST) {
            mip_type = MIP_ARP_REQUEST; 
        } else if (arp_type == ARP_TYPE_REPLY) {
            mip_type = MIP_ARP_REPLY;
        } else {
            if (debug_mode) {
                printf("Error: Unknown ARP type\n");
            }
            return -1;
        }
    } else if (pdu->miphdr->sdu_type == SDU_TYPE_PING) {
        if (pdu->sdu[1] == 0x50494E47) {
            mip_type = MIP_PING;
        } else if (pdu->sdu[1] == 0x504F4E47) {
            mip_type = MIP_PONG;
        } else {
            if (debug_mode) {
                printf("Error: Unknown PING type\n");
            }
            return -1;
        }

    // TODO: Fix type for ROUTE
    } else if (pdu->miphdr->sdu_type == SDU_TYPE_ROUTE) {
            return MIP_ROUTE;

    } else {
        if (debug_mode) {
            printf("Error: Unknown SDU type\n");
        }
        return -1;
    }   



    
    return mip_type;
}



/**
 * Handle messages received from an application and determine their type.
 *
 * fd: File descriptor from which to read the application message.
 * dst_mip_addr: Pointer to store the destination MIP address.
 * msg: Pointer to store the parsed message.
 * 
 * The function starts by initializing a buffer to read the message from the application.
 * It then reads the message and sets the destination MIP address based on the first byte 
 * of the received buffer.
 * 
 * The function identifies the type of application message by checking the prefix 
 * of the received message. Currently, it only identifies the APP_PING type based 
 * on the PING: prefix.
 * 
 * In case of read errors, the function prints an error message and exits the program.
 * 
 * Returns the type of the received application message.
 */

APP_handle handle_app_message(int app_fd, uint8_t *dst_mip_addr, char *msg, uint8_t *ttl)
{
    int rc;
    APP_handle app_type;
    
    // Buffer to hold message from application
    char buf[512];

    // Clear buffer
    memset(buf, 0, sizeof(buf));

    printf("Handle app message 1\n");
    // Read message from application
    rc = read(app_fd, buf, sizeof(buf));
    if (rc <= 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }
    printf("Handle app message 2\n");
    // Set the destination_mip to the first byte of the buffer
    *dst_mip_addr = (uint8_t) buf[0];
    *ttl = (uint8_t) buf[1];

    printf("Handle app message 3\n");
    // Initialize an offset for the message
    int offset = 2; // Skip the first byte (destination_mip)

    // Set app_type
    if (strncmp(buf + offset, "PING:", 5) == 0) {
        app_type = APP_PING;

    } else if (strncmp(buf + offset, "PONG:", 5) == 0) {
        app_type = APP_PONG;

    } else if (strncmp(buf + offset, "ROUTE:", 6) == 0) {
        app_type = APP_ROUTE;

    } else {
        perror("Unknown message type");
        close(app_fd);
        exit(EXIT_FAILURE);
    }
    // Copy the rest of the buffer to msg
    strcpy(msg, buf + offset);

    return app_type;

}






ROUTE_handle handle_route_message(int route_fd, uint8_t *buf, size_t buf_size)
{
    int rc;
    ROUTE_handle route_type;

    // Clear buffer
    memset(buf, 0, buf_size);

    // Read message from application
    rc = read(route_fd, buf, buf_size);
    if (rc <= 0) {
        perror("read");
        return -1; // Return an error code
    }

    if (buf[2] == 0x48 && buf[3] == 0x45 && buf[4] == 0x4C) {
        route_type = ROUTE_HELLO;
    } else if (buf[2] == 0x55 && buf[3] == 0x50 && buf[4] == 0x44) {
        route_type = ROUTE_UPDATE;
    } else if (buf[2] == 0x52 && buf[3] == 0x45 && buf[4] == 0x53) {
        route_type = ROUTE_RESPONSE;
    } else {
        perror("Unknown message type");
        return -1; // Return an error code
    }

    return route_type;
}


/**
 * Send a MIP packet using a raw socket.
 *
 * ifs: Pointer to the interface data structure.
 * src_mac_addr: Pointer to the source MAC address.
 * dst_mac_addr: Pointer to the destination MAC address.
 * src_mip_addr: Source MIP address.
 * dst_mip_addr: Destination MIP address.
 * ttl: Time-to-live value for the packet.
 * sdu_type: Type of service data unit.
 * sdu: Pointer to the service data unit payload.
 * sdu_len: Length of the service data unit payload.
 * 
 * The function begins by allocating a PDU (Protocol Data Unit) structure. 
 * It then fills this PDU with the provided details, including MAC addresses, 
 * MIP addresses, TTL, SDU type, and the SDU payload. Once the PDU is filled, 
 * it is serialized into a send buffer.
 * 
 * The function then finds the appropriate socket address structure 
 * for the source MAC address and sends the serialized buffer via a raw socket.
 * 
 * If in debug mode, the function prints details about the sent PDU.
 * 
 * The PDU is destroyed before exiting the function.
 * 
 * Returns 0 on successful execution. If there is an error in allocating the PDU, 
 * the function returns -ENOMEM.
 */
// int send_mip_packet(struct ifs_data *ifs,
//             uint8_t *src_mac_addr,
//             uint8_t *dst_mac_addr,
//             uint8_t src_mip_addr,
//             uint8_t dst_mip_addr,
//             uint8_t ttl,
//             uint8_t sdu_type,
//             const uint32_t *sdu,
//             uint16_t sdu_len)
// {
//     struct pdu *pdu = alloc_pdu();
//     uint8_t snd_buf[MAX_BUF_SIZE];
    
//     if (NULL == pdu)
//         return -ENOMEM;



//     fill_pdu(pdu, src_mip_addr, dst_mip_addr, ttl, sdu_type, sdu, sdu_len);

//     pdu->miphdr->ttl--;

//     size_t snd_len = mip_serialize_pdu(pdu, snd_buf);

//     /* Send the serialized buffer via RAW socket */

//     // Find matching interface
//     struct sockaddr_ll *interface = find_matching_sockaddr(ifs, src_mac_addr);


//     if (sendto(ifs->rsock, snd_buf, snd_len, 0,
//         (struct sockaddr *)interface,
//         sizeof(struct sockaddr_ll)) <= 0) {
//         perror("sendto()");
//         close(ifs->rsock);
//     }

//     if (debug_mode){
//         printf("Sending PDU with content (size %zu):\n", snd_len);
//         print_pdu_content(pdu);
//     }


//     destroy_pdu(pdu);
//     return 0;
// }


struct sockaddr_ll* find_matching_sockaddr(struct ifs_data *ifs, uint8_t *dst_mac_addr) {
    if (ifs == NULL || dst_mac_addr == NULL) {
        return NULL;
    }

    for (int i = 0; i < ifs->ifn; ++i) {
        uint8_t *mac_addr = ifs->addr[i].sll_addr;

        if (memcmp(mac_addr, dst_mac_addr, 6) == 0) {
            return &(ifs->addr[i]);
        }
    }

    return NULL; // Return NULL if not found
}

/**
 * Convert a string to an array of uint32_t values.
 *
 * str: Pointer to the input string.
 * length: Pointer to store the output length in bytes of the resulting array.
 * 
 * The function calculates the number of uint32_t elements required to represent the string. 
 * The first uint32_t in the resulting array stores the length of the input string.
 * The subsequent uint32_t values store the characters of the string, with up to 
 * 4 characters packed into each uint32_t.
 * 
 * For example, the string "ABCD" would be stored in one uint32_t with 'A' in the most significant byte 
 * and 'D' in the least significant byte.
 * 
 * The function returns a pointer to the dynamically allocated uint32_t array. 
 * The caller is responsible for freeing this memory when it's no longer needed.
 * If memory allocation fails, the function returns NULL.
 */

uint32_t* stringToUint32Array(const char* str, uint8_t *length) {
    uint8_t str_length = strlen(str);


    uint8_t num_elements = str_length / 4 + (str_length % 4 != 0) + 1;
    // Allocate memory for the array
    
    
    // Calculate length in bytes and set the output parameter
    *length = num_elements;


    
    uint32_t *arr = (uint32_t*)calloc(num_elements, sizeof(uint32_t));

    if (arr == NULL) {
        return NULL; // Failed to allocate memory
    }

    arr[0] = (uint32_t)str_length; // Store the length in bytes in the first uint32_t

    for (uint8_t i = 0; i < str_length; i++) {
        uint8_t arr_idx = i / 4 + 1; // Start from index 1
        uint8_t shift = (3 - (i % 4)) * 8;

        arr[arr_idx] |= (uint32_t)str[i] << shift;
    }
    

    return arr;
}

uint32_t find_matching_if_index(struct ifs_data *ifs, struct sockaddr_ll *from_addr) {
    for (int i = 0; i < ifs->ifn; i++) {
        if (ifs->addr[i].sll_ifindex == from_addr->sll_ifindex) {
            return i;
        }
    }
    return -1;
}

// Function to clear the ping_data struct
void clear_ping_data(struct ping_data *data) {
    if (data == NULL) {
        return;
    }
    data->dst_mip_addr = 0;
    memset(data->msg, 0, sizeof(data->msg));
}

void decode_sdu_miparp(uint32_t* sdu_array, uint8_t* mip_addr) {
    if (sdu_array == NULL || mip_addr == NULL) {
        return; // Error, some pointer is NULL
    }
    
    uint32_t sdu = sdu_array[0];
    
    // Extract mip_addr from bits 23-30
    *mip_addr = (sdu >> 23) & 0xFF;
}

void decode_fill_ping_buf(const char *buf, size_t buf_size, char *destination_host, char *message) {
    // Check for NULL pointers and empty buffer
    if (buf == NULL || buf_size == 0 || destination_host == NULL || message == NULL) {
        return;
    }

    // Extract ASCII code for destination_host
    char ascii_code = buf[0];
    sprintf(destination_host, "%c", ascii_code);

    // Find the start of the message after "PING:"
    const char *ping_start = strstr(buf, "PING:");
    if (ping_start != NULL) {
        // Extract the message
        strncpy(message, ping_start + strlen("PING:"), buf_size - (ping_start + strlen("PING:") - buf));
    }
}


char* uint32ArrayToString(uint32_t* arr) {
    if (arr == NULL) {
        return NULL;
    }

    uint32_t str_length = arr[0];

    char *str = (char*)calloc(str_length + 1, sizeof(char));

    if (str == NULL) {
        return NULL;
    }

    uint8_t num_elements = (str_length + 3) / 4 + 1;

    for (uint8_t i = 1; i < num_elements; i++) {
        uint32_t val = arr[i];

        for (int j = 3; j >= 0; j--) {
            uint8_t shift = j * 8;
            char ch = (char)((val >> shift) & 0xFF);

            if (str_length > 0) {
                *str = ch;
                str++;
                str_length--;
            }
        }
    }

    *str = '\0';
    return str - arr[0];
}

// TODO: remove
// uint8_t routing_lookup(uint8_t host_mip_addr, int *route_fd) {
//     uint8_t next_hop;

//     // Request message format
//     uint8_t request_msg[REQUEST_MSG_LEN] = {
//         host_mip_addr,
//         0x00, // TTL
//         0x52, // 'R'
//         0x45, // 'E'
//         0x51, // 'Q'
//         host_mip_addr // MIP address to look up
//     };

//     // Send request to routing daemon
//     ssize_t sent_bytes = send(*route_fd, request_msg, REQUEST_MSG_LEN, 0);
//     if (sent_bytes < 0) {
//         perror("send");
//         return 0; // 0 could signify an error
//     }

//     // Receive response from routing daemon
//     uint8_t response_msg[RESPONSE_MSG_LEN];
//     ssize_t received_bytes = recv(*route_fd, response_msg, RESPONSE_MSG_LEN, 0);
//     if (received_bytes < 0) {
//         perror("recv");
//         return 0; // 0 could signify an error
//     }

//     // Validate the response
//     if (received_bytes != RESPONSE_MSG_LEN || 
//         response_msg[2] != 0x52 || response_msg[3] != 0x53 || response_msg[4] != 0x50) {
//         fprintf(stderr, "Invalid response format.\n");
//         return 0; // 0 could signify an error
//     }

//     // Extract the next hop MIP address
//     next_hop = response_msg[5];
//     return next_hop;
// }

// Function to send ARP request to all interfaces
// void send_arp_request_to_all_interfaces(struct ifs_data *ifs, uint8_t target_mip_addr, int debug_mode) {
//     // Create SDU for ARP request
//     uint8_t sdu_len = 1 * sizeof(uint32_t); // MIP ARP SDU length is 1 uint32_t
//     uint32_t *sdu = create_sdu_miparp(ARP_TYPE_REQUEST, target_mip_addr);

//     // Create Broadcast MAC address
//     uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

//     // Create broadcast MIP address
//     uint8_t broadcast_mip_addr = 0xff;

//     // Send MIP packet to all interfaces
//     for (int interface = 0; interface < ifs->ifn; interface++) {
//         if (debug_mode) {
//             printf("Sending MIP_BROADCAST to MIP: %u on interface %d\n", broadcast_mip_addr, interface);
//         }
//         send_mip_packet(ifs, ifs->addr[interface].sll_addr, broadcast_mac, ifs->local_mip_addr, broadcast_mip_addr, 0, SDU_TYPE_MIPARP, sdu, sdu_len);
//     }

//     free(sdu);
// }

// void fill_forward_data(struct forward_data *forward_data, uint8_t next_hop_MIP, struct pdu *pdu, int *waiting_to_forward) {

//     // Set waiting_to_forward to 1
//     *waiting_to_forward = 1;


//     forward_data->next_hop_MIP = next_hop_MIP;
//     forward_data->ttl = pdu->miphdr->ttl;
//     forward_data->sdu_type = pdu->miphdr->sdu_type;

//     // Calculate the number of elements in the SDU array
//     size_t sdu_elements = pdu->miphdr->sdu_len;

//     // Allocate memory for the SDU and initialize it to zero
//     forward_data->sdu = (uint32_t *)calloc(sdu_elements, sizeof(uint32_t));
//     if (forward_data->sdu == NULL) {
//         perror("Failed to allocate memory for SDU");
//         exit(EXIT_FAILURE);
//     }

//     // Copy the SDU content
//     memcpy(forward_data->sdu, pdu->sdu, sdu_elements * sizeof(uint32_t));
//     forward_data->sdu_len = pdu->miphdr->sdu_len;
// }

// void clear_forward_data(struct forward_data *forward_data, int *waiting_to_forward) {

//     // Set waiting_to_forward to 0
//     *waiting_to_forward = 0;


//     // Free the dynamically allocated memory for the SDU
//     if (forward_data->sdu != NULL) {
//         free(forward_data->sdu);
//         forward_data->sdu = NULL; // Set pointer to NULL to avoid dangling pointer
//     }

//     // Reset other fields to default values
//     forward_data->next_hop_MIP = 0;
//     forward_data->ttl = 0;
//     forward_data->sdu_type = 0;
//     forward_data->sdu_len = 0;
// }

// Send message to neighbours

// void send_message_to_neighbours(struct ifs_data *ifs, uint8_t *src_mac_addr, uint8_t *dst_mac_addr, uint8_t src_mip_addr, uint8_t dst_mip_addr, uint8_t ttl, uint8_t sdu_type, const uint32_t *sdu, uint16_t sdu_len, int debug_mode) {
//     // Send MIP packet to all interfaces
//     for (int interface = 0; interface < ifs->ifn; interface++) {
//         if (debug_mode) {
//             printf("Sending MIP_BROADCAST to MIP: %u on interface %d\n", dst_mip_addr, interface);
//         }
//         send_mip_packet(ifs, src_mac_addr, dst_mac_addr, src_mip_addr, dst_mip_addr, ttl, sdu_type, sdu, sdu_len);
//     }
// }



// void sendToRoutingDaemon(void) {
//     printf("MADE");
// }

// void MIP_send(struct ifs_data *ifs, uint8_t dst_mip_addr, uint8_t ttl, const char* message, int type, struct pdu_queue *queue, int debug_mode) {
//     // Lookup the MAC address for the destination MIP address

//     if (dst_mip_addr == BROADCAST_MIP_ADDR){
//         // Send MIP packet to all interfaces
//         uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

//         uint8_t sdu_len;
//         uint32_t *sdu = stringToUint32Array(message, &sdu_len);


//         for (int interface = 0; interface < ifs->ifn; interface++) {
//             if (debug_mode) {
//                 printf("Sending MIP_BROADCAST to MIP: %u on interface %d\n", BROADCAST_MIP_ADDR, interface);
//             }
//             send_mip_packet(ifs, ifs->addr[interface].sll_addr, broadcast_mac, ifs->local_mip_addr, BROADCAST_MIP_ADDR, 0, type, sdu, sdu_len);
//         }
//     }else{

//         uint8_t sdu_len;
//         uint8_t *dst_mac_addr = arp_lookup(dst_mip_addr);
//         uint8_t interface = arp_lookup_interface(dst_mip_addr);
//         uint32_t *sdu = stringToUint32Array(message, &sdu_len);
//         if (dst_mac_addr) {
//             if (debug_mode) {
//                 printf("We have the MAC address for MIP %u\n", dst_mip_addr);
//             }

//             if (debug_mode) {
//                 printf("Sending MIP_PING to MIP: %u\n", dst_mip_addr);
//             }

//             // Subtract 1 from TTL to account for the current node

//             if (ttl > 0) {
//                 send_mip_packet(ifs, ifs->addr[interface].sll_addr, dst_mac_addr, ifs->local_mip_addr, dst_mip_addr, ttl, type, sdu, sdu_len*sizeof(uint32_t));
//             }
//             else {
//                 printf("TTL is 0, not sending packet\n");
//             }
            

//             free(sdu);

//         } else {
//             if (debug_mode) {
//                 printf("MAC address for MIP %u not found in cache\n", dst_mip_addr);
//             }

//             send_arp_request_to_all_interfaces(ifs, dst_mip_addr, debug_mode);


//             // Add to queue
//             struct pdu *pdu = alloc_pdu();
//             uint8_t sdu_len;
//             uint32_t *sdu = stringToUint32Array(message, &sdu_len);

//             fill_pdu(pdu, ifs.ifn[interface].sll_addr, dst_mac_addr, localMIP, dst_mip_addr, ttl, type, sdu, sdu_len*sizeof(uint32_t));
//             enqueue(queue, pdu);

//             free(sdu);

//         }
//     }
// }

struct pdu* create_PDU(uint8_t src_mip_addr,
            uint8_t dst_mip_addr,
            uint8_t ttl,
            uint8_t sdu_type,
            const uint32_t *sdu,
            uint16_t sdu_len)
{

    printf("inside create_PDU\n");
    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }
    struct pdu *pdu = alloc_pdu();
    fill_pdu(pdu, src_mip_addr, dst_mip_addr, ttl, sdu_type, sdu, sdu_len);

    return pdu;
}

void send_PDU(struct ifs_data *ifs, struct pdu *pdu, struct sockaddr_ll *interface) {
    uint8_t snd_buf[MAX_BUF_SIZE];
    printf("TTL23: %d\n", pdu->miphdr->ttl);
    // Check TTL
    if (pdu->miphdr->ttl < 0) {
        printf("TTL is 0, not sending packet\n");
        return;
    }



    // // Decrement TTL
    // pdu->miphdr->ttl--;

    // print ttl

    printf("minus shit\n");
    for (int i = 0; i < 2; i++){
        printf("%u ", pdu->sdu[i]);
    }


    size_t snd_len = mip_serialize_pdu(pdu, snd_buf);



    // Send the serialized buffer via RAW socket
    if (sendto(ifs->rsock, snd_buf, snd_len, 0,
        (struct sockaddr *)interface,
        sizeof(struct sockaddr_ll)) <= 0) {
        perror("sendto()");
        close(ifs->rsock);
    }

    if (debug_mode) {
        printf("Sending PDU with content (size %zu):\n", snd_len);
        print_pdu_content(pdu);
    }

    destroy_pdu(pdu);
}








void uint32_to_uint8(uint32_t *input, size_t input_size, uint8_t *output) {
    for (size_t i = 0; i < input_size; ++i) {
        uint32_t value = input[i];
        
        output[i * 4]     = (value >> 24) & 0xFF; // Extract the first byte
        output[i * 4 + 1] = (value >> 16) & 0xFF; // Extract the second byte
        output[i * 4 + 2] = (value >> 8) & 0xFF;  // Extract the third byte
        output[i * 4 + 3] = value & 0xFF;         // Extract the fourth byte
    }
}




uint32_t* uint8ArrayToUint32Array(const uint8_t* byte_array, uint8_t array_length, uint8_t *length) {
    uint8_t num_elements = array_length / 4 + (array_length % 4 != 0);

    // Calculate length in uint32_t elements and set the output parameter
    *length = num_elements;

    uint32_t *arr = (uint32_t*)calloc(num_elements, sizeof(uint32_t));
    if (arr == NULL) {
        return NULL; // Failed to allocate memory
    }


    for (uint8_t i = 0; i < array_length; i++) {
        uint8_t arr_idx = i / 4; // Start from index 1
        uint8_t shift = (3 - (i % 4)) * 8;

        arr[arr_idx] |= (uint32_t)byte_array[i] << shift;
    }

    return arr;
}
void sendRequestToApp(int route_fd, int destinationMIP, int localMIP) {
    printf("Looking up MIP: %d\n", destinationMIP);
    uint8_t requestMessage[] = {
        localMIP, // MIP address
        0x00,           // TTL set to zero
        0x52,           // ASCII for 'R'
        0x45,           // ASCII for 'E'
        0x51,           // ASCII for 'Q'
        destinationMIP        // Next hop for the destination
    };

    int bytes_sent = send(route_fd, requestMessage, sizeof(requestMessage), 0);
    if (bytes_sent < 0) {
        perror("send");
    }
}


void fill_ethhdr(struct pdu *pdu, const uint8_t *src_mac, const uint8_t *dst_mac) {
    if (!pdu || !pdu->ethhdr || !dst_mac || !src_mac) {
        printf("Invalid arguments\n");
        return;
    }

    memcpy(pdu->ethhdr->dst_mac, dst_mac, MAC_ADDR_SIZE);
    memcpy(pdu->ethhdr->src_mac, src_mac, MAC_ADDR_SIZE);
    pdu->ethhdr->ethertype = htons(ETH_P_MIP);
}