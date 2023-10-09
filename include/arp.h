#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_CACHE_SIZE 10
#define ARP_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

typedef struct {
    uint8_t ip;
    uint8_t mac[6];
} ArpEntry;

void print_arp_cache(ArpEntry *arp_cache);
void arp_init();
uint8_t* arp_lookup(uint8_t ip);
void arp_insert(uint8_t ip, uint8_t mac[6]);
void arp_broadcast(uint8_t target_ip);

#endif // ARP_H
