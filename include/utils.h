#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>

void print_mac_addr(uint8_t *, size_t);
void print_arp_cache();

#endif