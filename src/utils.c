#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "utils.c"
#include "arp.h"

// Print MAC address in hex format
void print_mac_addr(uint8_t *addr, size_t len)
{
	size_t i;

	for (i = 0; i < len - 1; i++) {
		printf("%02x:", addr[i]);
	}
	printf("%02x\n", addr[i]);
}



void print_arp_cache() {
    printf("ARP Cache:\n");
    for (int i = 0; i < ARP_CACHE_SIZE; ++i) {
        printf("IP: %u, MAC: ", arp_cache[i].ip);
        print_mac_addr(arp_cache[i].mac, 6);
        printf("\n");
    }
}
