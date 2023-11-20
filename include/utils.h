#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <limits.h>
#include <sys/un.h>

#include "ether.h"
#include "mip.h"
#include "arp.h"
#include "pdu.h"


#define INFINITY INT_MAX



#define MAX_EVENTS	10
#define MAX_IF		3



#define SDU_ARP_TYPE_LOOKUP 0
#define SDU_ARP_TYPE_MATCH  1

extern int debug_mode;
extern uint8_t localMIP;



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
    MIP_ARP_REPLY,
    MIP_ROUTE
} MIP_handle;

typedef enum {
    APP_PING,
    APP_PONG,
    APP_ROUTE
} APP_handle;

typedef enum {
    ROUTE_HELLO,
    ROUTE_UPDATE,
    ROUTE_RESPONSE
} ROUTE_handle;


void print_mac_addr(uint8_t *, size_t);
int create_raw_socket(void);
void get_mac_from_ifaces(struct ifs_data *);
void init_ifs(struct ifs_data *, int, uint8_t);
uint32_t* create_sdu_miparp(int arp_type, uint8_t mip_addr);

void fill_ping_buf(char *buf, size_t buf_size, const char *destination_host, const char *message, const char *ttl);
void fill_pong_buf(char *buf, size_t buf_size, const char *destination_host, const char *message);
MIP_handle handle_mip_packet(struct ifs_data *ifs, struct pdu *pdu, int *recv_ifs_index);
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
APP_handle handle_app_message(int app_fd, uint8_t *dst_mip_addr, char *msg, uint8_t *ttl);
struct sockaddr_ll* find_matching_sockaddr(struct ifs_data *ifs, uint8_t *dst_mac_addr);
uint32_t* stringToUint32Array(const char* str, uint8_t *length);
uint32_t find_matching_if_index(struct ifs_data *ifs, struct sockaddr_ll *from_addr);
void clear_ping_data(struct ping_data *data);
void decode_sdu_miparp(uint32_t* sdu_array, uint8_t* mip_addr);
void decode_fill_ping_buf(const char *buf, size_t buf_size, char *destination_host, char *message);
char* uint32ArrayToString(uint32_t* arr);
// uint8_t routing_lookup(uint8_t host_mip_addr, int *route_fd);
void send_arp_request_to_all_interfaces(struct ifs_data *ifs, uint8_t target_mip_addr, int debug_mode);
void fill_forward_data(struct forward_data *forward_data, uint8_t next_hop_MIP, struct pdu *pdu, int *waiting_to_forward);
void clear_forward_data(struct forward_data *forward_data, int *waiting_to_forward);
ROUTE_handle handle_route_message(int route_fd, uint8_t *msg);

void sendToRoutingDaemon(void);

// void MIP_send(struct ifs_data *ifs, uint8_t dst_mip_addr, uint8_t ttl, const char* message, int type, struct pdu_queue *queue, int debug_mode);
struct pdu* create_PDU(uint8_t src_mip_addr,
            uint8_t dst_mip_addr,
            uint8_t ttl,
            uint8_t sdu_type,
            const uint32_t *sdu,
            uint16_t sdu_len);

void send_PDU(struct ifs_data *ifs, struct pdu *pdu);

void uint32_to_uint8(uint32_t *input, size_t input_size, uint8_t *output);
uint32_t* uint8ArrayToUint32Array(const uint8_t* byte_array, uint8_t array_length, uint8_t *length);
void sendRequestToApp(int route_fd, int destinationMIP, int localMIP);
void fill_ethhdr(struct pdu *pdu, const uint8_t *dst_mac, const uint8_t *src_mac);
#endif