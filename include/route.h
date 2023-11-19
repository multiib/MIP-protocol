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

extern int neighborTable[MAX_NODES];
extern struct RoutingEntry routingTable[MAX_NODES];
extern struct NeighborStatus neighborStatus[MAX_NODES];

extern int routingTableHasChanged;

void initializeRoutingTable(struct RoutingEntry *table, int size);

struct RoutingEntry lookupRoutingEntry(int mipAddress, struct RoutingEntry* routingTable);

void updateRoutingTable(int sourceMIP, struct RoutingEntry receivedTable[MAX_NODES]);



void handleHelloMessage(int MIPgreeter);

void sendUpdateFromApp (int route_fd);



void sendHelloFromApp(int route_fd);

void handleIncomingMessages(int route_fd);
void checkForNeighborTimeouts(int route_fd);
void handleRequestMessage(int route_fd, uint8_t *requestMessage, int messageLength);
void handleUpdateMessage(uint8_t *updateMessage, int messageLength);
void sendResponseFromApp(int route_fd, int next_hop);

int getNextHopMIP(int destinationMIP);


void sendRequestToApp(int route_fd, int destinationMIP);
// void forward_pdu(struct pdu *pdu, struct pdu_queue *pdu_queue);

#endif