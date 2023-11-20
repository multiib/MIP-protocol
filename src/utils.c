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



/**
 * Create a raw socket for sending and receiving packets.
 * 
 * This function sets up a raw socket using the AF_PACKET family, which is used for 
 * sending and receiving packets at the device driver (OSI Layer 2) level. It uses the 
 * SOCK_RAW type and specifies the ETH_P_MIP protocol for the Ethernet protocol field. 
 * The function handles socket creation errors by printing an error message and exiting 
 * the program.
 * 
 * Returns the socket descriptor if successful, otherwise terminates the program.
 */
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

/**
 * Retrieve MAC addresses from network interfaces and store them in a struct.
 * 
 * ifs: Pointer to a struct ifs_data where interface information will be stored.
 * 
 * This function enumerates network interfaces, excluding the loopback interface, 
 * and stores their MAC addresses in the provided 'ifs_data' struct. It uses 
 * 'getifaddrs' to dynamically allocate a list of interfaces and iterates through 
 * this list to copy MAC address information into 'ifs'. The function ensures to 
 * free the dynamically allocated memory after processing. It handles errors in 
 * 'getifaddrs' by printing an error message and exiting the program.
 * 
 * Note: The function assumes 'ifs' has enough space to store the addresses and updates 
 * the 'ifn' member of 'ifs' to reflect the number of interfaces processed.
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

/**
 * Initialize network interface data structure.
 * 
 * ifs: Pointer to a struct ifs_data to be initialized.
 * rsock: The RAW socket associated with the node.
 * mip_addr: The local MIP address of the node.
 * 
 * This function initializes a struct 'ifs_data' with network interface information.
 * It retrieves MAC addresses from local network interfaces and stores them in 'ifs'.
 * Additionally, it sets the RAW socket 'rsock' and the local MIP address 'mip_addr' 
 * in the 'ifs_data' struct.
 */
void init_ifs(struct ifs_data *ifs, int rsock, uint8_t mip_addr)
{
    /* Get some info about the local ifaces */
    get_mac_from_ifaces(ifs);

    /* We use one RAW socket per node */
    ifs->rsock = rsock;

    /* Set the local MIP address */
    ifs->local_mip_addr = mip_addr;
}

/**
 * Create a Single Data Unit (SDU) for MIP ARP request or response.
 * 
 * arp_type: An integer indicating ARP type (0 for request, 1 for response).
 * mip_addr: The MIP address to be included in the SDU.
 * 
 * This function allocates memory for an SDU array, sets the ARP type and MIP address
 * in the SDU, and returns a pointer to the allocated SDU array. The caller is responsible
 * for freeing the memory allocated for the SDU array when it is no longer needed.
 * 
 * Returns a pointer to the allocated SDU array or NULL if memory allocation fails.
 */
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

/**
 * Fill a buffer with a ping message.
 * 
 * buf: The buffer to be filled.
 * buf_size: The size of the buffer.
 * destination_host: The destination host identifier (usually an integer).
 * message: The message to be included in the ping (optional, can be NULL).
 * ttl: The Time to Live value for the ping packet.
 * 
 * This function initializes the buffer with zeros, sets the destination host and TTL,
 * and constructs a ping message in the format "PING:message" if a message is provided.
 * 
 * Note: Ensure that the buffer is large enough to accommodate the message.
 */
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

    uint8_t rcv_buf[512];
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

/**
 * Handle a route message received on a given socket file descriptor.
 * 
 * route_fd: The socket file descriptor for routing messages.
 * buf: The buffer to store the received message.
 * buf_size: The size of the buffer.
 * 
 * This function reads a message from the specified socket file descriptor and determines
 * its type based on the message header. It recognizes three types: HELLO, UPDATE, and RESPONSE.
 * The type is returned as a ROUTE_handle enum value.
 * 
 * Returns:
 *   - ROUTE_HELLO for hello messages
 *   - ROUTE_UPDATE for update messages
 *   - ROUTE_RESPONSE for response messages
 *   - -1 for unknown message types or read errors
 */
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
 * Find a matching sockaddr_ll structure based on the destination MAC address.
 * 
 * struct ifs_data *ifs: Pointer to the ifs_data structure containing interface information.
 * uint8_t *dst_mac_addr: Pointer to the destination MAC address to match.
 * 
 * Returns:
 * - Pointer to the matching sockaddr_ll structure if found, or NULL if not found.
 */
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