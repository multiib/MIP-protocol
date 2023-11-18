#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <errno.h>

#include "route.h"
#include "utils.h"
#include "arp.h"
#include "ether.h"
#include "pdu.h"
#include "mip.h"



void initializeRoutingTable(struct RoutingEntry* table, int size) {
    for (int i = 0; i < size; i++) {
        table[i].destination = i; // MIP address as the destination
        table[i].next_hop = -1;    // -1 indicates unknown next hop
        table[i].distance = INFINITY; // INFINITY for no known path
    }
}


struct RoutingEntry lookupRoutingEntry(int mipAddress, struct RoutingEntry* routingTable) {
    if (mipAddress >= 0 && mipAddress < MAX_NODES) {
        return routingTable[mipAddress];
    } else {
        struct RoutingEntry invalidEntry = {mipAddress, -1, INFINITY};
        return invalidEntry;
    }
}

void updateRoutingTable(int sourceMIP, struct RoutingEntry receivedTable[MAX_NODES]) {
    for (int i = 0; i < MAX_NODES; i++) {
        int newDistance = receivedTable[i].distance + lookupRoutingEntry(sourceMIP, receivedTable).distance;
        if (newDistance < routingTable[i].distance) {
            routingTable[i].distance = newDistance;
            routingTable[i].next_hop = sourceMIP;
        }
    }
}



void handleHelloMessage(int MIPgreeter) {

    // Check if the senderMIP is within valid range
    if (MIPgreeter >= 0 && MIPgreeter < MAX_NODES) {
        // Mark this MIP address as a neighbor
        neighborTable[MIPgreeter] = 1;

        // If this is a new neighbor, initialize a direct route in the routing table
        if (routingTable[MIPgreeter].next_hop == -1) {
            routingTable[MIPgreeter].next_hop = MIPgreeter;
            routingTable[MIPgreeter].distance = 1; // Cost is always 1 for a direct route
        }
    }
}

void sendRoutingUpdate(int socket_fd) {
    uint8_t updateMessage[3 * MAX_NODES + 5]; // Header + 5 bytes for each entry
    updateMessage[0] = localMIP; // MIP address of the sender
    updateMessage[1] = 0x00;     // TTL set to zero
    updateMessage[2] = 0x55;     // 'U'
    updateMessage[3] = 0x50;     // 'P'
    updateMessage[4] = 0x44;     // 'D'

    int messageLength = 5; // Header length

    for (int i = 0; i < MAX_NODES; i++) {
        if (routingTable[i].next_hop != -1) {
            updateMessage[messageLength++] = routingTable[i].destination;
            updateMessage[messageLength++] = routingTable[i].next_hop;
            updateMessage[messageLength++] = routingTable[i].distance;
        }
    }

    int bytes_sent = send(socket_fd, updateMessage, messageLength, 0);
    if (bytes_sent < 0) {
        perror("send");
    } else {
        printf("Routing update sent.\n");
    }
}


void handleUpdateMessage(uint8_t *updateMessage, int messageLength) {
    if (messageLength < 4) { // Check for minimum length (header + at least one entry)
        printf("Invalid update message length.\n");
        return;
    }

    uint8_t senderMIP = updateMessage[0];
    // Assuming updateMessage[1], updateMessage[2], and updateMessage[3] are 'U', 'P', 'D'
    
    int index = 4; // Start reading after the header
    while (index + 2 < messageLength) {
        uint8_t destination = updateMessage[index++];
        uint8_t next_hop = updateMessage[index++];
        uint8_t distance = updateMessage[index++];

        // Poisoned Reverse Check: Ignore routes where this node is the next hop
        if (next_hop == localMIP) {
            continue;
        }

        // Update the routing table
        int newDistance = distance + 1; // Assuming direct link cost to senderMIP is 1
        if (newDistance < routingTable[destination].distance) {
            routingTable[destination].distance = newDistance;
            routingTable[destination].next_hop = senderMIP;
        }
    }
}


void sendHelloMessage(int socket_fd) {
    uint8_t helloMessage[] = {
        localMIP, // MIP address
        0x00,           // TTL set to zero
        0x48,           // ASCII for 'H'
        0x45,           // ASCII for 'E'
        0x4C            // ASCII for 'L'
    };

    int bytes_sent = send(socket_fd, helloMessage, sizeof(helloMessage), 0);
    if (bytes_sent < 0) {
        perror("send");
    } else {
        printf("Hello message sent.\n");
    }
}


void handleIncomingMessages(int socket_fd) {
    uint8_t read_buf[1024];
    int rc = read(socket_fd, read_buf, sizeof(read_buf));
    if (rc < 0) {
        perror("read");
        close(socket_fd);
        exit(EXIT_FAILURE);
    } else {
        printf("Received %d bytes.\n", rc);
    }

    if (read_buf[2] == 0x48 && read_buf[3] == 0x45 && read_buf[4] == 0x4C) {
        handleHelloMessage(read_buf[0]);
    } else if (read_buf[2] == 0x55 && read_buf[3] == 0x50 && read_buf[4] == 0x44) {
        handleUpdateMessage(read_buf, rc);
    } else if (read_buf[2] == 0x52 && read_buf[3] == 0x45 && read_buf[4] == 0x51) {
        handleRequestMessage(socket_fd, read_buf, rc);
    } else {
        printf("Invalid message type.\n");

        // Quit if the message type is invalid
        close(socket_fd);
        exit(EXIT_FAILURE);

    }

}

void checkForNeighborTimeouts(int socket_fd) {
    time_t currentTime = time(NULL);
    for (int i = 0; i < MAX_NODES; i++) {
        if (neighborTable[i] && (currentTime - neighborStatus[i].lastHelloReceived > TIMEOUT_INTERVAL)) {
            neighborTable[i] = 0;
            neighborStatus[i].isReachable = 0;
            routingTable[i].next_hop = -1;
            routingTable[i].distance = INFINITY;
            
            // Send routing update
            sendRoutingUpdate(socket_fd);
        }
    }
}

void handleRequestMessage(int socket_fd, uint8_t *requestMessage, int messageLength) {
    if (messageLength < 9) { // Check for minimum length (header + at least one entry)
        printf("Invalid request message length.\n");
        return;
    }

    uint8_t destinationMIP = requestMessage[5];
    // Assuming requestMessage[2], requestMessage[3], and requestMessage[4] are 'R', 'E', 'Q'
    
    uint8_t next_hop = getNextHopMIP(destinationMIP);
    sendResponseMessage(socket_fd, destinationMIP, next_hop);
}

int getNextHopMIP(int destinationMIP) {
    if (destinationMIP >= 0 && destinationMIP < MAX_NODES) {
        struct RoutingEntry entry = lookupRoutingEntry(destinationMIP, routingTable);
        if (entry.distance != INFINITY) {
            return entry.next_hop;  // Return the next hop for the destination
        }
    }
    return 255;  // Return 255 if the destination is invalid or unreachable
}

void sendResponseMessage(int socket_fd, int destinationMIP, int next_hop) {
    uint8_t responseMessage[] = {
        destinationMIP, // MIP address
        0x00,           // TTL set to zero
        0x52,           // ASCII for 'R'
        0x45,           // ASCII for 'E'
        0x53,           // ASCII for 'S'
        next_hop        // Next hop for the destination
    };

    int bytes_sent = send(socket_fd, responseMessage, sizeof(responseMessage), 0);
    if (bytes_sent < 0) {
        perror("send");
    } else {
        printf("Response message sent.\n");
    }
}