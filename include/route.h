#ifndef _ROUTE_H
#define _ROUTE_H

#include <stdint.h>
#include <unistd.h>
#include <linux/if_packet.h>

#include "ether.h"
#include "mip.h"
#include "arp.h"
#include "pdu.h"

#define MAX_NODES 52 // Maximum number of nodes in the network
#define TIMEOUT_INTERVAL 30 // Seconds

typedef struct {
    int destination;
    int next_hop;
    int distance;
} RoutingEntry;

typedef struct {
    time_t lastHelloReceived;
    int isReachable;
} NeighborStatus;

void initializeRoutingTable(RoutingEntry *table, int size);

RoutingEntry lookupRoutingEntry(int mipAddress);

void updateRoutingTable(int sourceMIP, RoutingEntry receivedTable[MAX_NODES]);



void handleHelloMessage(int MIPgreeter);

void sendRoutingUpdate(int socket_fd, int localMIP);

void receiveAndUpdateRoutingTable(uint8_t *updateMessage, int messageLength);

void sendHelloMessage(int socket_fd, uint8_t MIP_addr);

void handleIncomingMessages(int socket_fd);


#endif