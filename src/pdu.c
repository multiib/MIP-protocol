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

/**
 * Allocate memory for a Protocol Data Unit (PDU) structure and its components.
 * 
 * This function allocates memory for a PDU structure, an Ethernet header (eth_hdr), 
 * and a MIP header (mip_hdr), initializing their fields appropriately. It ensures 
 * that memory is successfully allocated for each component. If memory allocation 
 * fails at any point, it cleans up previously allocated memory before returning NULL.
 * 
 * The Ethernet header's ethertype is set to ETH_P_MIP, and MAC addresses are initialized to 0.
 * The MIP header fields are initialized to 0, and the Service Data Unit (sdu) pointer is set to NULL.
 * 
 * Returns a pointer to the allocated PDU structure, or NULL if any memory allocation fails.
 */
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

/**
 * Populate a PDU structure with provided data.
 * 
 * pdu: Pointer to the PDU structure to be filled.
 * src_mip_addr: Source MIP address.
 * dst_mip_addr: Destination MIP address.
 * ttl: Time To Live value.
 * sdu_type: Service Data Unit (SDU) type.
 * sdu: Pointer to the SDU data.
 * sdu_len: Length of the SDU data.
 * 
 * This function fills in the MIP header of the provided PDU structure with 
 * the specified source and destination MIP addresses, TTL, SDU type, and SDU length. 
 * If sdu is provided, it allocates memory for and copies the SDU data into the PDU.
 * Note: If the PDU's existing SDU memory is already allocated, it is freed first.
 * 
 * Note: The function does not return a value. It performs error handling for null 
 * pointers and memory allocation failures internally.
 * 
 * Note: The function does not populate the Ethernet header of the PDU.
 */
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

    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }
    if (pdu->sdu) {
        free(pdu->sdu); // Free existing sdu memory
    }

    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }

    pdu->sdu = (uint32_t *)calloc(sdu_len, sizeof(uint32_t));
    if (!pdu->sdu) {
        // Handle memory allocation failure
        return;
    }
    
    memcpy(pdu->sdu, sdu, sdu_len * sizeof(uint32_t));


    for (int i = 0; i < sdu_len; i++){
        printf("%u ", sdu[i]);
    }
}

/**
 * Serialize a PDU structure into a byte buffer for sending.
 * 
 * pdu: Pointer to the PDU structure to be serialized.
 * snd_buf: Buffer to store the serialized PDU.
 * 
 * This function serializes the given PDU structure into a byte buffer. It first copies 
 * the Ethernet header, then constructs and copies the MIP header, and finally attaches 
 * the SDU (Service Data Unit) if available. The MIP header fields are converted from 
 * host byte order to network byte order during serialization. The function calculates 
 * and returns the total length of the serialized data.
 * 
 * Returns the total length of the serialized PDU data.
 */
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

/**
 * Deserialize a byte buffer into a PDU structure.
 * 
 * pdu: Pointer to the PDU structure where the deserialized data will be stored.
 * rcv_buf: Buffer containing the serialized PDU data.
 * 
 * This function deserializes the data from the rcv_buf into the provided PDU structure.
 * It allocates memory and extracts the Ethernet header, MIP header, and SDU (if present), 
 * converting fields from network byte order to host byte order where necessary. The function
 * handles memory allocation failures and ensures proper cleanup in such cases.
 * 
 * Returns the total length of the deserialized data, or 0 if memory allocation fails.
 */
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

/**
 * Print the content of a PDU structure.
 * 
 * pdu: Pointer to the PDU structure whose contents are to be printed.
 * 
 * This function prints the details of the Ethernet header, including source and destination MAC 
 * addresses and ethertype, as well as the MIP header details like source and destination MIP 
 * addresses, TTL, SDU length, and SDU type. If an SDU is present, its content is printed as a 
 * series of uint32_t numbers. The output is formatted for readability with headers and separators.
 * 
 * Note: The function relies on 'print_mac_addr' to print MAC addresses.
 */
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
    for (int i = 0; i < pdu->miphdr->sdu_len; i++) {
        printf("%u ", pdu->sdu[i]);
    }
    printf("\n");
    printf("====================================================\n");
}

/**
 * Free the memory allocated for a PDU structure and its components.
 * 
 * pdu: Pointer to the PDU structure to be destroyed.
 * 
 * This function frees the memory allocated for the Ethernet header, MIP header, 
 * and SDU (if present) within the PDU. It also frees the memory allocated for 
 * the PDU structure itself. After freeing each component, it sets the corresponding 
 * pointers to NULL to avoid dangling pointer issues.
 */
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


/**
 * Initialize the queue for storing PDUs awaiting ARP responses.
 * 
 * This function sets up a queue designed to hold PDUs that are pending an ARP response.
 * It iterates over each slot in the queue, represented by the global array 'queue_arp',
 * and initializes it. For each slot, the 'packet' pointer is set to NULL, indicating that 
 * the slot is empty, and the 'is_occupied' flag is set to 0, denoting that the slot is not 
 * currently in use. The size of the queue is governed by the constant MAX_QUEUE_SIZE.
 * 
 * Note: This function is typically called during initialization to prepare the queue for use.
 */
void initialize_queue_arp() {
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        queue_arp[i].packet = NULL;
        queue_arp[i].is_occupied = 0;
    }
}

/**
 * Enqueue a PDU packet in the ARP queue.
 * 
 * packet: Pointer to the PDU packet to be enqueued.
 * next_hop: The next hop MIP address for the packet.
 * 
 * This function finds the first unoccupied slot in the 'queue_arp' array and 
 * enqueues the provided PDU packet. It sets the 'next_hop' for the packet and 
 * marks the slot as occupied. The queue's capacity is determined by MAX_QUEUE_SIZE.
 * 
 * Note: The function does not return a value and does not handle the scenario where 
 * the queue is full. It is assumed that the queue has enough space for the new packet.
 */
void enqueue_arp(struct pdu* packet, uint8_t next_hop) {
    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        if (!queue_arp[i].is_occupied) {
            queue_arp[i].packet = packet;
            queue_arp[i].next_hop = next_hop;  // Set the next hop
            queue_arp[i].is_occupied = 1;
        }
    }
}


/**
 * Remove and return a packet from the ARP queue based on a MAC address.
 * 
 * mac_address: MAC address used to identify the packet to be removed.
 * 
 * This function iterates through the 'queue_arp' array, searching for a packet whose 
 * destination MAC address matches the provided 'mac_address'. Upon finding such a packet, 
 * the function removes it from the queue and returns it along with its associated next hop. 
 * The packet and next hop are wrapped in a 'pdu_with_hop' structure. If no matching packet is found, 
 * the function returns a 'pdu_with_hop' structure initialized with NULL for the packet and 0 for the next hop.
 * 
 * Returns a 'pdu_with_hop' structure containing the packet and its next hop, or NULL and 0 if no match is found.
 */
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





/**
 * Initialize a FIFO queue for PDUs waiting for DVR replies.
 * 
 * queue: Pointer to the queue structure to be initialized.
 * 
 * This function initializes the provided FIFO queue structure, setting the front and rear 
 * pointers to NULL, indicating an empty queue. It also sets the size of the queue to 0. This 
 * queue is specifically used to store PDUs that are waiting for Dynamic Virtual Routing (DVR) replies.
 * 
 * Note: This function assumes that the queue structure has already been allocated.
 */
void initialize_queue_forward(struct queue_f* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
}

/**
 * Enqueue a PDU packet in a FIFO queue for PDUs awaiting DVR replies.
 * 
 * queue: Pointer to the FIFO queue where the PDU is to be enqueued.
 * packet: Pointer to the PDU packet to be enqueued.
 * 
 * This function creates a new queue node and enqueues the provided PDU packet into the 
 * specified FIFO queue. It handles the case where the queue is initially empty, as well 
 * as when it already contains packets. The function increases the queue size upon successful 
 * enqueueing. In case of a memory allocation failure for the new queue node, the function 
 * returns -1.
 * 
 * Returns 0 on successful enqueue, -1 on memory allocation failure.
 */
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

/**
 * Dequeue a PDU packet from a FIFO queue used for PDUs awaiting DVR replies.
 * 
 * queue: Pointer to the FIFO queue from which the PDU is to be dequeued.
 * 
 * This function removes and returns the PDU packet at the front of the specified FIFO queue. 
 * It handles the queue's internal pointers and updates its size accordingly. If the queue is 
 * empty, the function returns NULL. After removing the front node, it frees the memory allocated 
 * for that node and adjusts the front and rear pointers of the queue if needed.
 * 
 * Returns a pointer to the dequeued PDU packet, or NULL if the queue is empty.
 */
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