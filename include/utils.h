#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>

#define MAX_EVENTS	10
#define MAX_IF		3

struct ifs_data {
	struct sockaddr_ll addr[MAX_IF];
	int rsock;
	int ifn;
};

void print_mac_addr(uint8_t *, size_t);
void print_arp_cache();
int create_raw_socket(void);
void get_mac_from_ifaces(struct ifs_data *);
void init_ifs(struct ifs_data *, int);

#endif