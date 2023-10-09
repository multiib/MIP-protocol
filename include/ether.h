#ifndef _ETHER_H
#define _ETHER_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>

#define MAC_ADDR_SIZE 6
#define ETH_HDR_LEN sizeof(struct eth_hdr)

struct eth_hdr {
	uint8_t  dst_mac[MAC_ADDR_SIZE];
	uint8_t  src_mac[MAC_ADDR_SIZE];
	uint16_t ethertype;
} __attribute__((packed));

#endif