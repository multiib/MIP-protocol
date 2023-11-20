#include "arp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

static ArpEntry arp_cache[ARP_CACHE_SIZE];
static int arp_count;


/**
 * Display the ARP cache entries.
 * 
 * arp_cache: Pointer to an array of ArpEntry structures.
 * Iterates through each entry in the ARP cache (of size ARP_CACHE_SIZE), printing 
 * the Machine IP (MIP) and corresponding MAC address for each entry.
 * 
 * Note: Uses 'print_mac_addr' to print MAC addresses, assuming MAC_ADDR_SIZE length.
 */
void print_arp_cache(ArpEntry *arp_cache) {
    printf("ARP Cache:\n");
    for (int i = 0; i < ARP_CACHE_SIZE; ++i) {
        printf("MIP: %u, MAC: ", arp_cache[i].mip);
        print_mac_addr(arp_cache[i].mac, MAC_ADDR_SIZE);
        printf("\n");
    }
}

/**
 * Initialize the ARP table count.
 * 
 * This function sets the global variable 'arp_count' to 0, effectively
 * initializing the count of ARP entries in the table.
 */
void arp_init() {
    arp_count = 0;
}

/**
 * Look up a MAC address in the ARP cache using a given MIP.
 * 
 * mip: MIP for which the MAC address needs to be found.
 * This function iterates over the entries in the ARP cache, 
 * comparing the MIP of each entry with the provided mip. 
 * If a match is found, it returns a pointer to the corresponding MAC address.
 * 
 * Returns a pointer to the MAC address if found, or NULL if no match is found.
 */
uint8_t* arp_lookup(uint8_t mip) {



    for (int i = 0; i < arp_count; ++i) {
        if (arp_cache[i].mip == mip) {
            return arp_cache[i].mac;
        }
    }
    return NULL;
}

/**
 * Retrieve the interface associated with a given MIP from the ARP cache.
 * 
 * mip: MIP to search for in the ARP cache.
 * Iterates over ARP cache entries to find an entry matching the given MIP.
 * If a match is found, returns the associated interface index.
 * 
 * Returns the interface index if a match is found, or -1 if no match is found.
 */
uint8_t arp_lookup_interface(uint8_t mip) {
    for (int i = 0; i < arp_count; ++i) {
        if (arp_cache[i].mip == mip) {
            return arp_cache[i].interface;
        }
    }
    return -1;
}

/**
 * Insert a new entry into the ARP cache.
 * 
 * mip: MIP to be added to the ARP cache.
 * mac: MAC address corresponding to the MIP, represented as an array of 6 bytes.
 * interface: Interface index associated with the MIP.
 * 
 * This function adds a new entry to the ARP cache if there is space available 
 * (determined by ARP_CACHE_SIZE). It copies the MIP, MAC address, and interface
 * index into the next available slot in the cache and increments the arp_count.
 */
void arp_insert(uint8_t mip, uint8_t mac[6], int interface) {
    if (arp_count < ARP_CACHE_SIZE) {
        arp_cache[arp_count].mip = mip;
        memcpy(arp_cache[arp_count].mac, mac, 6);
        arp_cache[arp_count].interface = interface;
        arp_count++;
    }
}