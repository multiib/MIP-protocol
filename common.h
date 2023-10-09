#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>		/* uint8_t */
#include <unistd.h>		/* size_t */
#include <linux/if_packet.h>	/* struct sockaddr_ll */

#define MAX_EVENTS	10
#define MAX_IF		3
#define ETH_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}



struct ether_frame {
    uint8_t dst_addr : 8;     // Destination MIP address
    uint8_t src_addr : 8;     // Source MIP address
    uint8_t ttl : 4;          // Time To Live
    uint16_t : 9;             // SDU length
    uint8_t sdu_type : 3;     // SDU type
} __attribute__((packed));

struct ifs_data {
	struct sockaddr_ll addr[MAX_IF];
	int rsock[MAX_IF];
	int ifn;
};

void init_ifs(struct ifs_data *);
void print_mac_addr(uint8_t *, size_t);
// void init_ifs(struct ifs_data *, int);
int create_raw_socket(void);
int send_arp_request(struct ifs_data *);
int send_arp_response(struct ifs_data *,
		      struct sockaddr_ll *,
		      struct ether_frame);
int handle_arp_packet(struct ifs_data *);

#endif /* _COMMON_H */
