#ifndef _MIP_H_
#define _MIP_H_

#include <stdint.h>
#include <stddef.h>



#define MIP_HDR_LEN	sizeof(struct mip_hdr)

#define HIP_VERSION	4

#define BROADCAST_MIP_ADDR ((uint8_t)0xFF)

#define MIP_DST_ADDR	0xff

struct mip_hdr {
    uint8_t dst : 8;     // Destination MIP address
    uint8_t src : 8;     // Source MIP address
    uint8_t ttl : 4;          // Time To Live
    uint16_t sdu_len : 9;     // SDU length
    uint8_t sdu_type : 3;     // SDU type
} __attribute__((packed));


#endif /* _MIP_H_ */
