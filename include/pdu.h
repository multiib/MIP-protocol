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

struct pdu {
    struct eth_hdr *ethhdr;
    struct mip_hdr *miphdr;
    uint8_t        *sdu;
} __attribute__((packed));

struct pdu * alloc_pdu(void);
void fill_pdu(struct pdu *pdu,
              uint8_t *src_mac_addr,
              uint8_t *dst_mac_addr,
              uint8_t src_mip_addr,
              uint8_t dst_mip_addr,
              uint8_t ttl,
              uint8_t sdu_type,
              const char *sdu);
size_t mip_serialize_pdu(struct pdu *, uint8_t *);
size_t mip_deserialize_pdu(struct pdu *, uint8_t *);
void print_pdu_content(struct pdu *);
void destroy_pdu(struct pdu *);

#endif /* _PDU_H_ */
