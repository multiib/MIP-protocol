#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_CACHE_SIZE 10
#define ARP_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

typedef struct {
    uint8_t mip;
    uint8_t mac[6];
} ArpEntry;

void print_arp_cache(ArpEntry *);
void arp_init();
uint8_t* arp_lookup(uint8_t);
void arp_insert(uint8_t, uint8_t);
void arp_broadcast(uint8_t);

#endif // ARP_H
