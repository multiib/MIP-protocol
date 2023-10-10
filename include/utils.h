#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>

#define MAX_EVENTS	10
#define MAX_IF		3

#define MAC_ADDR_SIZE 6

#define SDU_TYPE_LOOKUP 0
#define SDU_TYPE_MATCH  1

struct ifs_data {
	struct sockaddr_ll addr[MAX_IF];
	int rsock;
	int ifn;
    uint8_t local_mip_addr;
};

void print_mac_addr(uint8_t *, size_t);
int create_raw_socket(void);
void get_mac_from_ifaces(struct ifs_data *);
void init_ifs(struct ifs_data *, int, uint8_t);
u_int32_t create_sdu_miparp(int, uint8_t);
int add_to_epoll_table(int, int);
void handle_client(int);

#endif