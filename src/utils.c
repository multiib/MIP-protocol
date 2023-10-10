#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>


#include "utils.h"
#include "arp.h"
#include "ether.h"


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
	short unsigned int protocol = ETH_P_MIP;

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

u_int32_t create_sdu_miparp(int sdu_type, uint8_t mip_addr){
    u_int32_t sdu = 0;
    if (sdu_type){
        sdu |= (1 << 31);
    }

    sdu |= (mip_addr << 23);
    return sdu;
}

int add_to_epoll_table(int efd, int fd)
{
		int rc = 0;

		struct  epoll_event ev;
		
		ev->events = EPOLLIN;
		ev->data.fd = fd;
		if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
				perror("epoll_ctl");
				rc = -1;
		}

		return rc;
}

void handle_client(int fd)
{
		char buf[256];
		int rc;

		/* The memset() function fills the first 'sizeof(buf)' bytes
		 * of the memory area pointed to by 'buf' with the constant byte 0.
		 */
		memset(buf, 0, sizeof(buf));

		/* read() attempts to read up to 'sizeof(buf)' bytes from file
		 * descriptor fd into the buffer starting at buf.
		 */
		rc = read(fd, buf, sizeof(buf));
		if (rc <= 0) {
				close(fd);
				printf("<%d> left the chat...\n", fd);
				return;
		}

		printf("<%d>: %s\n", fd, buf);
}