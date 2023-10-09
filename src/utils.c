#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <stdint.h>

#include "utils.h"
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




/* Prepare RAW socket */
int create_raw_socket(void)
{
	int sd;
	short unsigned int protocol = 0xFFFF;

	/* Set up a raw AF_PACKET socket without ethertype filtering */
	sd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
	if (sd == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	return sd;
}

/*
 * This function stores struct sockaddr_ll addresses for all interfaces of the
 * node (except loopback interface)
 */
void get_mac_from_ifaces(struct ifs_data *ifs)
{
        struct ifaddrs *ifaces, *ifp;
        int i = 0;

        /* Enumerate interfaces: */
        /* Note in man getifaddrs that this function dynamically allocates
           memory. It becomes our responsability to free it! */
        if (getifaddrs(&ifaces)) {
                perror("getifaddrs");
                exit(-1);
        }

        /* Walk the list looking for ifaces interesting to us */
        for (ifp = ifaces; ifp != NULL; ifp = ifp->ifa_next) {
                /* We make sure that the ifa_addr member is actually set: */
                if (ifp->ifa_addr != NULL &&
                    ifp->ifa_addr->sa_family == AF_PACKET &&
                    strcmp("lo", ifp->ifa_name))
			/* Copy the address info into the array of our struct */
                        memcpy(&(ifs->addr[i++]),
                               (struct sockaddr_ll*)ifp->ifa_addr,
                               sizeof(struct sockaddr_ll));
        }
        /* After the for loop, the address info of all interfaces are stored */
        /* Update the counter of the interfaces */
        ifs->ifn = i;

        /* Free the interface list */
        freeifaddrs(ifaces);
}


void init_ifs(struct ifs_data *ifs, int rsock, uint8_t mip_addr)
{

	/* Get some info about the local ifaces */
	get_mac_from_ifaces(ifs);

	/* We use one RAW socket per node */
	ifs->rsock = rsock;
	
	/* One MIP address per node; We name nodes and not interfaces like the
	 * Internet does. Read about RINA Network Architecture for more info
	 * about what's wrong with the current Internet.
	 */


	ifs->local_mip_addr = mip_addr;
}

u_int32_t create_sdu(int sdu_type, uint8_t mip_addr){
    u_int32_t sdu = 0;
    if (sdu_type){
        sdu |= (1 << 31);
    }

    sdu |= (mip_addr << 23);
    return sdu;
}

int epoll_add_sock(int sd)
{
	struct epoll_event ev;

	/* Create epoll table */
	int epollfd = epoll_create1(0);
	if (epollfd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	/* Add RAW socket to epoll table */
	ev.events = EPOLLIN;
	ev.data.fd = sd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sd, &ev) == -1) {
		perror("epoll_ctl: raw_sock");
		exit(EXIT_FAILURE);
	}

	return epollfd;
}