#ifndef _MIP_H_
#define _MIP_H_

#include <stdint.h>
#include <stddef.h>

#define HIP_HDR_LEN	sizeof(struct mip_hdr)

#define HIP_VERSION	4

#define ARP_TYPE_LOOKUP 0
#define ARP_TYPE_MATCH  1

#define MIP_DST_ADDR	0xff

struct mip_hdr {
    uint8_t dst_addr : 8;     // Destination MIP address
    uint8_t src_addr : 8;     // Source MIP address
    uint8_t ttl : 4;          // Time To Live
    uint16_t sdu_len : 9;     // SDU length
    uint8_t sdu_type : 3;     // SDU type
} __attribute__((packed));


#endif /* _MIP_H_ */
