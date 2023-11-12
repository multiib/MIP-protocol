#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>

#include "ether.h"
#include "mip.h"
#include "arp.h"
#include "pdu.h"



#define MAX_EVENTS	10
#define MAX_IF		3



#define SDU_ARP_TYPE_LOOKUP 0
#define SDU_ARP_TYPE_MATCH  1

extern int debug_mode;

struct ifs_data {
    struct sockaddr_ll addr[MAX_IF];
    int rsock;
    int ifn;
    uint8_t local_mip_addr;
};

typedef enum {
    MIP_PING,
    MIP_PONG,
    MIP_ARP_REQUEST,
    MIP_ARP_REPLY
} MIP_handle;

typedef enum {
    APP_PING,
    APP_PONG
} APP_handle;

void print_mac_addr(uint8_t *, size_t);
int create_raw_socket(void);
void get_mac_from_ifaces(struct ifs_data *);
void init_ifs(struct ifs_data *, int, uint8_t);
uint32_t* create_sdu_miparp(int arp_type, uint8_t mip_addr);
int add_to_epoll_table(int, int);
void fill_ping_buf(char *, size_t, const char *, const char *);
void fill_pong_buf(char *, size_t, const char *, const char *);
MIP_handle handle_mip_packet(int raw_fd, struct ifs_data *ifs, struct pdu *pdu, int *recv_ifs_index);
int send_mip_packet(struct ifs_data *ifs,
                    uint8_t *src_mac_addr,
                    uint8_t *dst_mac_addr,
                    uint8_t src_mip_addr,
                    uint8_t dst_mip_addr,
                    uint8_t ttl,
                    uint8_t sdu_type,
                    const uint32_t *sdu,
                    uint16_t sdu_len);
//HANDLE
APP_handle handle_app_message(int fd, uint8_t *dst_mip_addr, char *msg);
struct sockaddr_ll* find_matching_sockaddr(struct ifs_data *ifs, uint8_t *dst_mac_addr);
uint32_t* stringToUint32Array(const char* str, uint8_t *length);
uint32_t find_matching_if_index(struct ifs_data *ifs, struct sockaddr_ll *from_addr);
void clear_ping_data(struct ping_data *data);
void decode_sdu_miparp(uint32_t* sdu_array, uint8_t* mip_addr);
void decode_fill_ping_buf(const char *buf, size_t buf_size, char *destination_host, char *message);
char* uint32ArrayToString(uint32_t* arr);


#endif