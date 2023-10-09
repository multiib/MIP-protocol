#include "arp.h"
#include <stdlib.h>
#include <string.h>
#include "utils.h"

static ArpEntry arp_cache[ARP_CACHE_SIZE];
static int arp_count = 0;

void print_arp_cache(ArpEntry *arp_cache) {
    printf("ARP Cache:\n");
    for (int i = 0; i < ARP_CACHE_SIZE; ++i) {
        printf("IP: %u, MAC: ", arp_cache[i].ip);
        print_mac_addr(arp_cache[i].mac, MAC_ADDR_SIZE);
        printf("\n");
    }
}

void arp_init() {
    arp_count = 0;
}

uint8_t* arp_lookup(uint8_t ip) {
    for (int i = 0; i < arp_count; ++i) {
        if (arp_cache[i].ip == ip) {
            return arp_cache[i].mac;
        }
    }
    return NULL;
}

void arp_insert(uint8_t ip, uint8_t mac[6]) {
    if (arp_count < ARP_CACHE_SIZE) {
        arp_cache[arp_count].ip = ip;
        memcpy(arp_cache[arp_count].mac, mac, 6);
        arp_count++;
    }
    // TODO: Handle cache eviction if needed
}

void arp_broadcast(uint8_t target_ip){
    
}