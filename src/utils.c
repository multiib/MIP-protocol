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





// Print MAC address in hex format
void print_mac_addr(uint8_t *addr, size_t len)
{
    size_t i;

    for (i = 0; i < len - 1; i++) {
        printf("%02x:", addr[i]);
    }
    printf("%02x\n", addr[i]);
}




/* Prepare RAW socket */
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


void init_ifs(struct ifs_data *ifs, int rsock, uint8_t mip_addr)
{

    /* Get some info about the local ifaces */
    get_mac_from_ifaces(ifs);

    /* We use one RAW socket per node */
    ifs->rsock = rsock;
    
    /* One MIP address per node; We name nodes and not interfaces like the
     * Internet does. Read about RINA Network Architecture for more info
     * about what's wrong with the current Internet.
     */


    ifs->local_mip_addr = mip_addr;
}

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
int add_to_epoll_table(int efd, int fd)
{
        int rc = 0;

        struct  epoll_event ev;
        
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                perror("epoll_ctl");
                rc = -1;
        }

        return rc;
}


void fill_ping_buf(char *buf, size_t buf_size, const char *destination_host, const char *message) {
    // Initialize the buffer to zeros
    memset(buf, 0, buf_size);

    buf[0] = atoi(destination_host);

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

    // Add "PING:"
    strcat(buf, "PONG:");

    // Then add message
    if (message != NULL) {
        strcat(buf, message);
    }
}


MIP_handle handle_mip_packet(int raw_fd, struct ifs_data *ifs, struct pdu *pdu, int *recv_ifs_index)
{
    // Make sure pdu is not NULL
    if (pdu == NULL) {
        perror("NULL pdu argument");
        return -EINVAL;
    }

    MIP_handle mip_type;

    uint8_t rcv_buf[256];
    struct sockaddr_ll from_addr;
    socklen_t from_addr_len = sizeof(from_addr);
    
    /* Recv the serialized buffer via RAW socket */
    if (recvfrom(ifs->rsock, rcv_buf, 256, 0, (struct sockaddr *)&from_addr, &from_addr_len) <= 0) {
        perror("recvfrom()");
        close(ifs->rsock);
        return -1;
    }

    *recv_ifs_index = find_matching_if_index(ifs, &from_addr);

    size_t rcv_len = mip_deserialize_pdu(pdu, rcv_buf);

    if (pdu->miphdr->sdu_type == SDU_TYPE_MIPARP) {
        int arp_type = (pdu->sdu[0] >> 31) & 1;

        if (arp_type == ARP_TYPE_REQUEST) {
            mip_type = MIP_ARP_REQUEST; 
        } else if (arp_type == ARP_TYPE_REPLY) {
            mip_type = MIP_ARP_REPLY;
        } else {
            printf("Error: Unknown ARP type\n");
            return -1;
        }
    } else if (pdu->miphdr->sdu_type == SDU_TYPE_PING) {
        if (pdu->sdu[1] == 0x50494E47) {
            mip_type = MIP_PING;
        } else if (pdu->sdu[1] == 0x504F4E47) {
            mip_type = MIP_PONG;
        } else {
            printf("Error: Unknown SDU\n");
            return -1;
        }
    }

    printf("Receiving PDU with content (size %zu) :\n", rcv_len);
    print_pdu_content(pdu);
    
    return mip_type;
}


// void send_broadcast(struct ifs_data *ifs, uint8_t *src_mac_addr, uint8_t src_mip_addr, const char *sdu)
// {
//     uint8_t broadcast_mac_addr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
//     send_mip_packet(ifs, src_mac_addr, broadcast_mac_addr, src_mip_addr, 0, sdu, 0);
// }


/// HANDLE ERA


APP_handle handle_app_message(int fd, uint8_t *dst_mip_addr, char *msg)
{
    int rc;
    APP_handle app_type;
    
    // Buffer to hold message from application
    char buf[256];

    // Clear buffer
    memset(buf, 0, sizeof(buf));

    // Read message from application
    rc = read(fd, buf, sizeof(buf));
    if (rc <= 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    // Set the destination_mip to the first byte of the buffer
    *dst_mip_addr = (uint8_t) buf[0];
    printf("Destination MIP: %d\n", *dst_mip_addr); //////DEBUG

    // Initialize an offset for the message
    int offset = 1; // Skip the first byte (destination_mip)
    printf("Buffer content at offset: %s\n", buf + offset);
    // Set app_type
    if (strncmp(buf + offset, "PING:", 5) == 0) {
        app_type = APP_PING;

    } else if (strncmp(buf + offset, "PONG:", 5) == 0) {
        app_type = APP_PONG;

    } else {
        perror("Unknown message type");
        close(fd);
        exit(EXIT_FAILURE);
    }
    // Copy the rest of the buffer to msg
    strcpy(msg, buf + offset);

    return app_type;

}


int send_mip_packet(struct ifs_data *ifs,
            uint8_t *src_mac_addr,
            uint8_t *dst_mac_addr,
            uint8_t src_mip_addr,
            uint8_t dst_mip_addr,
            uint8_t ttl,
            uint8_t sdu_type,
            const uint32_t *sdu,
            uint8_t sdu_len)
{
    struct pdu *pdu = alloc_pdu();
    uint8_t snd_buf[MAX_BUF_SIZE];
    
    if (NULL == pdu)
        return -ENOMEM;

    fill_pdu(pdu, src_mac_addr, dst_mac_addr, src_mip_addr, dst_mip_addr, ttl, sdu_type, sdu, sdu_len);

    size_t snd_len = mip_serialize_pdu(pdu, snd_buf);

    /* Send the serialized buffer via RAW socket */

    // Find matching interface
    struct sockaddr_ll *interface = find_matching_sockaddr(ifs, src_mac_addr);


    if (sendto(ifs->rsock, snd_buf, snd_len, 0,
        (struct sockaddr *)interface,
        sizeof(struct sockaddr_ll)) <= 0) {
        perror("sendto()");
        close(ifs->rsock);
    }






    printf("Sending PDU with content (size %zu):\n", snd_len);
    print_pdu_content(pdu);

    destroy_pdu(pdu);
    return 0;
}


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

// This function will convert a string into an array of uint32_t
uint32_t* stringToUint32Array(const char* str, uint8_t *length) {
    uint8_t str_length = strlen(str);
    uint8_t num_elements = (str_length + 3) / 4 + 1; // Calculate the number of uint32_t elements needed, +1 for the length

    // Calculate length in bytes and set the output parameter
    *length = num_elements * sizeof(uint32_t);

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

void decode_sdu_miparp(uint32_t* sdu_array, int* arp_type, uint8_t* mip_addr) {
    if (sdu_array == NULL || arp_type == NULL || mip_addr == NULL) {
        return; // Error, some pointer is NULL
    }
    
    uint32_t sdu = sdu_array[0];
    
    // Extract arp_type from bit 31
    *arp_type = (sdu >> 31) & 1;
    
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

// Reverse function without length parameter
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
