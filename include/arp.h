#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_CACHE_SIZE 10


#define ARP_TYPE_REQUEST 0
#define ARP_TYPE_REPLY   1


#define CLIENT 0
#define SERVER 1

typedef struct {
    uint8_t mip;
    uint8_t mac[6];
    uint8_t interface;
} ArpEntry;

void print_arp_cache(ArpEntry *);
void arp_init();
uint8_t* arp_lookup(uint8_t);
uint8_t arp_lookup_interface(uint8_t);
void arp_insert(uint8_t, uint8_t[6], int interface);
void arp_broadcast(uint8_t);
int arp_count_entries();
uint8_t* arp_get_mip_addresses();
uint8_t arp_get_mip_from_interface(uint8_t interface);

#endif // ARP_H
