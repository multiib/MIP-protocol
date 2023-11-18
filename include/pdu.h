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



struct forward_data{
    uint8_t next_hop_MIP;       // Next hop MIP address
    uint8_t ttl;                // Time To Live
    uint8_t sdu_type;           // Service Data Unit type
    uint32_t *sdu;              // Pointer to Service Data Unit array
    size_t sdu_len;             // Length of the SDU array
};

struct pdu_node {
    struct pdu *packet;
    struct pdu_node *next;
};

struct pdu_queue {
    struct pdu_node *front;
    struct pdu_node *rear;
    int size;
};


struct pdu * alloc_pdu(void);
void fill_pdu(struct pdu *pdu,
              uint8_t *src_mac_addr,
              uint8_t *dst_mac_addr,
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
void initialize_queue(struct pdu_queue *queue);
int is_queue_empty(struct pdu_queue *queue);
void enqueue(struct pdu_queue *queue, struct pdu *packet);
struct pdu * dequeue(struct pdu_queue *queue);
void clear_ping_data(struct ping_data *data);
void forward_pdu(int fd, struct pdu *pdu, struct pdu_queue *pdu_queue);


#endif /* _PDU_H_ */
