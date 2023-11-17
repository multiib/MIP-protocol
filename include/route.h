#ifndef _ROUTE_H
#define _ROUTE_H

#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <linux/if_packet.h>

#include "ether.h"
#include "mip.h"
#include "arp.h"
#include "pdu.h"

#define MAX_NODES 52 // Maximum number of nodes in the network
#define TIMEOUT_INTERVAL 30 // Seconds


struct RoutingEntry {
    int destination;
    int next_hop;
    int distance;
};

struct NeighborStatus {
    time_t lastHelloReceived;
    int isReachable;
};



void initializeRoutingTable(struct RoutingEntry *table, int size);

struct RoutingEntry lookupRoutingEntry(int mipAddress, struct RoutingEntry *routingTable);

void updateRoutingTable(int sourceMIP, struct RoutingEntry receivedTable[MAX_NODES]);



void handleHelloMessage(int MIPgreeter);

void sendRoutingUpdate(int socket_fd, int localMIP);



void sendHelloMessage(int socket_fd, uint8_t MIP_addr);

void handleIncomingMessages(int socket_fd);
void checkForNeighborTimeouts(int socket_fd, int localMIP);
void handleRequestMessage(int socket_fd, uint8_t *requestMessage, int messageLength)

#endif