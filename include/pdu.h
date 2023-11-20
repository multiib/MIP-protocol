#ifndef _PDU_H_
#define _PDU_H_

#include <stdint.h>
#include <stddef.h>

#include "ether.h"
#include "mip.h"

#define MIP_HDR_LEN	sizeof(struct mip_hdr)
#define MAX_BUF_SIZE	1024

#define SDU_TYPE_MIPARP 0x01
#define SDU_TYPE_PING   0x02
#define SDU_TYPE_ROUTE  0x04

#define MAX_RETURN_SIZE 4
#define MAX_QUEUE_SIZE 8



struct pdu {
    struct eth_hdr *ethhdr;
    struct mip_hdr *miphdr;
    uint32_t        *sdu;
} __attribute__((packed));

struct ping_data {
    uint8_t dst_mip_addr;
    uint8_t ttl;
    char   msg[512];
};

 struct pdu_queue_slot {
    struct pdu* packet;
    uint8_t next_hop;  // New field for the next hop
    int is_occupied;
};

struct pdu_with_hop {
    struct pdu* packet;
    uint8_t next_hop;
};

struct queue_node {
    struct pdu* packet;
    struct queue_node* next;
};

struct queue_f {
    struct queue_node* front;
    struct queue_node* rear;
    int size;
};

extern struct pdu_queue_slot queue_arp[MAX_QUEUE_SIZE];

struct pdu * alloc_pdu(void);
void fill_pdu(struct pdu *pdu,
              uint8_t src_mip_addr,
              uint8_t dst_mip_addr,
              uint8_t ttl,
              uint8_t sdu_type,
              const uint32_t *sdu,
              uint16_t sdu_len);
size_t mip_serialize_pdu(struct pdu *, uint8_t *);
size_t mip_deserialize_pdu(struct pdu *, uint8_t *);
void print_pdu_content(struct pdu *);
void destroy_pdu(struct pdu *);
void initialize_queue_arp();
void enqueue_arp(struct pdu* packet, uint8_t next_hop);
struct pdu_with_hop remove_packet_by_mac(uint8_t* mac_address);

void clear_ping_data(struct ping_data *data);
void initialize_queue_forward(struct queue_f* queue);
int enqueue_forward(struct queue_f* queue, struct pdu* packet);
struct pdu* dequeue_forward(struct queue_f* queue);
#endif /* _PDU_H_ */
