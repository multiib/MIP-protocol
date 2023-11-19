#include "arp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

static ArpEntry arp_cache[ARP_CACHE_SIZE];
static int arp_count;


// Print the ARP cache
void print_arp_cache(ArpEntry *arp_cache) {
    printf("ARP Cache:\n");
    for (int i = 0; i < ARP_CACHE_SIZE; ++i) {
        printf("MIP: %u, MAC: ", arp_cache[i].mip);
        print_mac_addr(arp_cache[i].mac, MAC_ADDR_SIZE);
        printf("\n");
    }
}

// Initialize the ARP cache
void arp_init() {
    arp_count = 0;
}

// Lookup a MIP address in the ARP cache
uint8_t* arp_lookup(uint8_t mip) {



    for (int i = 0; i < arp_count; ++i) {
        if (arp_cache[i].mip == mip) {
            return arp_cache[i].mac;
        }
    }
    return NULL;
}

// Lookup the interface of a MIP address in the ARP cache
uint8_t arp_lookup_interface(uint8_t mip) {
    for (int i = 0; i < arp_count; ++i) {
        if (arp_cache[i].mip == mip) {
            return arp_cache[i].interface;
        }
    }
    return -1;
}

// Insert a MIP address and MAC address into the ARP cache
void arp_insert(uint8_t mip, uint8_t mac[6], int interface) {
    if (arp_count < ARP_CACHE_SIZE) {
        arp_cache[arp_count].mip = mip;
        memcpy(arp_cache[arp_count].mac, mac, 6);
        arp_cache[arp_count].interface = interface;
        arp_count++;
    }
}


// Return number of entries in ARP cache by counting the number of non-zero MIP addresses
int arp_count_entries() {
    int count = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; ++i) {
        if (arp_cache[i].mip != 0) {
            count++;
        }
    }
    return count;
}

// Get every mip address in the ARP cache
uint8_t* arp_get_mip_addresses() {
    uint8_t* mip_addresses = (uint8_t*)malloc(arp_count * sizeof(uint8_t));
    for (int i = 0; i < arp_count; ++i) {
        mip_addresses[i] = arp_cache[i].mip;
    }
    return mip_addresses;
}


// TODO: Create hash table for ARP cache



// Function to get MIP address from an interface
uint8_t arp_get_mip_from_interface(uint8_t interface) {
    for (int i = 0; i < arp_count; ++i) {
        if (arp_cache[i].interface == interface) {
            return arp_cache[i].mip;
        }
    }
    return -1; // Indicate that the interface was not found
}