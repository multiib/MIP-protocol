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

const char* create_sdu_miparp(int arp_type, uint8_t mip_addr) {
    u_int32_t sdu = 0;
    static char sdu_buffer[sizeof(u_int32_t)]; // Made static to preserve data after function returns

    if (arp_type) {
        sdu |= (1 << 31);
    }

    sdu |= (mip_addr << 23);

    // Copy the u_int32_t value into the buffer
    memcpy(sdu_buffer, &sdu, sizeof(u_int32_t));

    return sdu_buffer;
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

    // Add destination_host to buf
    if (destination_host != NULL) {
        strcpy(buf, destination_host);
    }

    // Add "PING:"
    strcat(buf, "PING:");

    // Then add message
    if (message != NULL) {
        strcat(buf, message);
    }
}


MIP_handle handle_mip_packet(int raw_fd, struct ifs_data *ifs)
{

    MIP_handle mip_type;
    struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));
    if (NULL == pdu) {
        perror("malloc");
        return -ENOMEM;
    }
    
    uint8_t rcv_buf[256];
    
    /* Recv the serialized buffer via RAW socket */
    if (recvfrom(ifs->rsock, rcv_buf, 256, 0, NULL, NULL) <= 0) {
        perror("recvfrom()");
        close(ifs->rsock);
    }

    size_t rcv_len = mip_deserialize_pdu(pdu, rcv_buf);

    // Create Broadcast MAC address
    uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    if (pdu->miphdr->sdu_type == SDU_TYPE_MIPARP){
        if (pdu->ethhdr->dst_mac == broadcast_mac){
            mip_type = MIP_ARP_REQUEST; 
        } else {
            mip_type = MIP_ARP_REPLY;
        }

    }else if (pdu->miphdr->sdu_type == SDU_TYPE_PING){
    
        if (strncmp(pdu->sdu + 1, "PING:", 5) == 0) {
            mip_type = MIP_PING;

        } else if (strncmp(pdu->sdu + 1, "PONG:", 5) == 0) {
            mip_type = MIP_PONG;
        }
    }
    

    printf("Receiving PDU with content (size %zu) :\n", rcv_len);
    print_pdu_content(pdu);

    destroy_pdu(pdu);
    
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

    // Initialize an offset for the message
    int offset = 1; // Skip the first byte (destination_mip)

    // Set app_type
    if (strncmp(buf + offset, "PING:", 5) == 0) {
        app_type = APP_PING;
        offset += 5;
    } else if (strncmp(buf + offset, "PONG:", 5) == 0) {
        app_type = APP_PONG;
        offset += 5;
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
                const char *sdu)
{
    struct pdu *pdu = alloc_pdu();
    uint8_t snd_buf[MAX_BUF_SIZE];
    
    if (NULL == pdu)
        return -ENOMEM;

    fill_pdu(pdu, src_mac_addr, dst_mac_addr, src_mip_addr, dst_mip_addr, ttl, sdu_type, sdu);

    size_t snd_len = mip_serialize_pdu(pdu, snd_buf);

    /* Send the serialized buffer via RAW socket */

    uint8_t interface = arp_lookup_interface(dst_mip_addr);
    
    if (sendto(ifs->rsock, snd_buf, snd_len, 0,
           (struct sockaddr *) &(ifs->addr[interface]),
           sizeof(struct sockaddr_ll)) <= 0) {
        perror("sendto()");
        close(ifs->rsock);
    }


    if (sendto(ifs->rsock, snd_buf, snd_len, 0,
           (struct sockaddr *) &(ifs->addr[0]),
           sizeof(struct sockaddr_ll)) <= 0) {
        perror("sendto()");
        close(ifs->rsock);
    }

    printf("Sending PDU with content (size %zu):\n", snd_len);
    print_pdu_content(pdu);

    destroy_pdu(pdu);
    return 0;
}