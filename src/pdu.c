#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>


#include "ether.h"
#include "mip.h"
#include "pdu.h"
#include "utils.h"
#include "arp.h"
#include "route.h"

struct pdu_queue_slot queue_arp[MAX_QUEUE_SIZE];

struct pdu * alloc_pdu(void) {
    struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));
    if (!pdu) {
        // Handle memory allocation failure
        return NULL;
    }
    
    pdu->ethhdr = (struct eth_hdr *)malloc(sizeof(struct eth_hdr));
    if (!pdu->ethhdr) {
        // Handle memory allocation failure
        free(pdu);
        return NULL;
    }
    pdu->ethhdr->ethertype = htons(ETH_P_MIP);
    // Initialize MAC addresses to 0
    memset(pdu->ethhdr->dst_mac, 0, 6);
    memset(pdu->ethhdr->src_mac, 0, 6);
    
    pdu->miphdr = (struct mip_hdr *)malloc(sizeof(struct mip_hdr));
    if (!pdu->miphdr) {
        // Handle memory allocation failure
        free(pdu->ethhdr);
        free(pdu);
        return NULL;
    }
    pdu->miphdr->dst = 0;
    pdu->miphdr->src = 0;
    pdu->miphdr->ttl = 0x00;
    pdu->miphdr->sdu_len = 0;
    pdu->miphdr->sdu_type = 0;

    pdu->sdu = NULL; // Initialize sdu pointer to NULL

    return pdu;
}


void fill_pdu(struct pdu *pdu,
              uint8_t src_mip_addr,
              uint8_t dst_mip_addr,
              uint8_t ttl,
              uint8_t sdu_type,
              const uint32_t *sdu,
              uint16_t sdu_len) {
    if (!pdu) {
        // Handle null pdu pointer
        return;
    }

    pdu->miphdr->dst = dst_mip_addr;
    pdu->miphdr->src = src_mip_addr;
    pdu->miphdr->ttl = ttl;
    pdu->miphdr->sdu_type = sdu_type;
    pdu->miphdr->sdu_len = sdu_len;
    printf("before free\n");
    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }
    if (pdu->sdu) {
        free(pdu->sdu); // Free existing sdu memory
    }
    printf("after free\n");
    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }

    pdu->sdu = (uint32_t *)calloc(sdu_len, sizeof(uint32_t));
    if (!pdu->sdu) {
        // Handle memory allocation failure
        return;
    }
    
    memcpy(pdu->sdu, sdu, sdu_len * sizeof(uint32_t));

    printf("last free\n");
    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }
}


size_t mip_serialize_pdu(struct pdu *pdu, uint8_t *snd_buf)
{
    size_t snd_len = 0;

    /* Copy ethernet header */
    memcpy(snd_buf + snd_len, pdu->ethhdr, sizeof(struct eth_hdr));
    snd_len += ETH_HDR_LEN;

    /* Copy MIP header */
    uint32_t miphdr = 0;
    miphdr |= (uint32_t) pdu->miphdr->dst << 24;
    miphdr |= (uint32_t) pdu->miphdr->src << 16;
    miphdr |= (uint32_t) (pdu->miphdr->ttl & 0xff) << 12;
    miphdr |= (uint32_t) (pdu->miphdr->sdu_len & 0xff) << 3;
    miphdr |= (uint32_t) (pdu->miphdr->sdu_type & 0xf);

    /* prepare it to be sent from host to network */
    miphdr = htonl(miphdr);

    memcpy(snd_buf + snd_len, &miphdr, MIP_HDR_LEN);
    snd_len += MIP_HDR_LEN;

    /* Attach SDU */
    memcpy(snd_buf + snd_len, pdu->sdu, pdu->miphdr->sdu_len);
    snd_len += pdu->miphdr->sdu_len;



    return snd_len;
}

size_t mip_deserialize_pdu(struct pdu *pdu, uint8_t *rcv_buf) {
    size_t rcv_len = 0;

    // Unpack ethernet header
    pdu->ethhdr = (struct eth_hdr *)malloc(ETH_HDR_LEN);
    if (!pdu->ethhdr) {
        // Handle memory allocation failure
        return 0;
    }
    memcpy(pdu->ethhdr, rcv_buf + rcv_len, ETH_HDR_LEN);
    rcv_len += ETH_HDR_LEN;

    pdu->miphdr = (struct mip_hdr *)malloc(MIP_HDR_LEN);
    if (!pdu->miphdr) {
        // Handle memory allocation failure
        free(pdu->ethhdr);
        return 0;
    }
    uint32_t *tmp = (uint32_t *) (rcv_buf + rcv_len);
    uint32_t header = ntohl(*tmp);
    pdu->miphdr->dst = (uint8_t) (header >> 24);
    pdu->miphdr->src = (uint8_t) (header >> 16);
    pdu->miphdr->ttl = (uint8_t) (((header >> 12) & 0xf));
    pdu->miphdr->sdu_len = (uint8_t) ((header >> 3) & 0x3f);
    pdu->miphdr->sdu_type = (uint8_t) (header & 0xf);
    rcv_len += MIP_HDR_LEN;

    pdu->sdu = (uint32_t *)calloc(pdu->miphdr->sdu_len, sizeof(uint32_t));
    if (!pdu->sdu) {
        // Handle memory allocation failure
        free(pdu->miphdr);
        free(pdu->ethhdr);
        return 0;
    }
    memcpy(pdu->sdu, rcv_buf + rcv_len, pdu->miphdr->sdu_len * sizeof(uint32_t));
    rcv_len += pdu->miphdr->sdu_len * sizeof(uint32_t);

    return rcv_len;
}


void print_pdu_content(struct pdu *pdu)
{
    printf("====================================================\n");
    printf("\t Source MAC address: ");
    print_mac_addr(pdu->ethhdr->src_mac, 6);
    printf("\t Destination MAC address: ");
    print_mac_addr(pdu->ethhdr->dst_mac, 6);
    printf("\t Ethertype: 0x%04x\n", pdu->ethhdr->ethertype);

    printf("\t Source MIP address: %u\n", pdu->miphdr->src);
    printf("\t Destination MIP address: %u\n", pdu->miphdr->dst);
    printf("\t TTL: %u\n", pdu->miphdr->ttl);
    printf("\t SDU length: %u\n", pdu->miphdr->sdu_len);
    printf("\t SDU type: %u\n", pdu->miphdr->sdu_type);

    // Print SDU in uint32 numbers
    printf("\t SDU: ");
    for (int i = 0; i < pdu->miphdr->sdu_len/4; i++) {
        printf("%u ", pdu->sdu[i]);
    }
    printf("\n");
    printf("====================================================\n");
}

void destroy_pdu(struct pdu *pdu)
{   

    free(pdu->ethhdr);
    pdu->ethhdr = NULL;

    free(pdu->miphdr);
    pdu->miphdr = NULL;

    if (pdu->sdu != NULL){
        free(pdu->sdu);
        pdu->sdu = NULL;
    }
    free(pdu);
    pdu = NULL;

}
 

void initialize_queue_arp() {
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue_arp[i].packet = NULL;
        queue_arp[i].is_occupied = 0;
    }
}

void enqueue_arp(struct pdu* packet, uint8_t next_hop) {
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        if (!queue_arp[i].is_occupied) {
            queue_arp[i].packet = packet;
            queue_arp[i].next_hop = next_hop;  // Set the next hop
            queue_arp[i].is_occupied = 1;
        }
    }
}



struct pdu_with_hop remove_packet_by_mac(uint8_t* mac_address) {
    struct pdu_with_hop result;
    result.packet = NULL; // Initialize to NULL
    result.next_hop = 0;  // Initialize with a default value

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        if (queue_arp[i].is_occupied && memcmp(queue_arp[i].packet->ethhdr->dst_mac, mac_address, MAC_ADDR_SIZE) == 0) {
            result.packet = queue_arp[i].packet;
            result.next_hop = queue_arp[i].next_hop;

            queue_arp[i].packet = NULL;
            queue_arp[i].is_occupied = 0;

            return result; // Return the found packet and next_hop
        }
    }
    return result; // No packet found, return the initialized result
}




// Usage
// int count;
// struct pdu** pdus = remove_packets_by_mac(mac_address, &count);

// for (int i = 0; i < count; i++) {
//     struct pdu* pdu = pdus[i];
//     // Process pdu
// }


void initialize_queue_forward(struct queue_f* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

int enqueue_forward(struct queue_f* queue, struct pdu* packet) {
    struct queue_node* newNode = (struct queue_node*)malloc(sizeof(struct queue_node));
    if (!newNode) return -1;  // Memory allocation failed

    newNode->packet = packet;
    newNode->next = NULL;

    if (queue->rear == NULL) {  // If queue is empty
        queue->front = queue->rear = newNode;
    } else {
        queue->rear->next = newNode;
        queue->rear = newNode;
    }

    queue->size++;
    return 0;  // Success
}

struct pdu* dequeue_forward(struct queue_f* queue) {
    if (queue->front == NULL) return NULL;  // Queue is empty

    struct queue_node* temp = queue->front;
    struct pdu* packet = temp->packet;

    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    free(temp);
    queue->size--;
    return packet;
}