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



extern int route_fd;

/**
 * Initialize a routing table with default values.
 * 
 * table: Pointer to the routing table array to be initialized.
 * size: The number of entries in the routing table.
 * 
 * This function initializes each entry in the provided routing table. For each entry, 
 * it sets the destination to the index value (assuming MIP address as the destination), 
 * the next hop to -1 (indicating an unknown next hop), and the distance to INFINITY 
 * (representing no known path to the destination).
 * 
 * Note: The table is assumed to be pre-allocated with the specified size.
 */
void initializeRoutingTable(struct RoutingEntry* table, int size) {
    for (int i = 0; i < size; i++) {
        table[i].destination = i; // MIP address as the destination
        table[i].next_hop = -1;    // -1 indicates unknown next hop
        table[i].distance = INFINITY; // INFINITY for no known path
    }
}

/**
 * Look up a routing entry in the routing table based on a MIP address.
 * 
 * mipAddress: The MIP address for which the routing entry is to be found.
 * routingTable: Pointer to the routing table array.
 * 
 * This function searches the routing table for an entry corresponding to the specified 
 * MIP address. If the MIP address is within the valid range (0 to MAX_NODES - 1), it returns 
 * the corresponding entry from the routing table. If the MIP address is out of range, it creates 
 * and returns an 'invalid' routing entry with the destination set to the MIP address, next hop 
 * to -1, and distance to INFINITY, indicating an invalid or unknown route.
 * 
 * Returns the found RoutingEntry, or an invalid entry if the MIP address is out of range.
 */
struct RoutingEntry lookupRoutingEntry(int mipAddress, struct RoutingEntry* routingTable) {
    if (mipAddress >= 0 && mipAddress < MAX_NODES) {
        return routingTable[mipAddress];
    } else {
        struct RoutingEntry invalidEntry = {mipAddress, -1, INFINITY};
        return invalidEntry;
    }
}

/**
 * Update the local routing table based on received routing information.
 * 
 * sourceMIP: The MIP address of the source node from which the routing table is received.
 * receivedTable: The routing table received from the source node.
 * 
 * This function updates the local routing table by considering the routing information 
 * received from a source node. For each entry in the received routing table, it calculates 
 * the new distance to the destination node as the sum of the distance from the source node 
 * and the distance from the current node to the source node. If this new distance is shorter 
 * than the existing distance in the local routing table, the function updates the entry with 
 * the new distance and sets the next hop to the source MIP. This is a common operation in 
 * distance-vector routing protocols.
 * 
 * Note: The function modifies the global 'routingTable' directly.
 */
void updateRoutingTable(int sourceMIP, struct RoutingEntry receivedTable[MAX_NODES]) {
    for (int i = 0; i < MAX_NODES; i++) {
        int newDistance = receivedTable[i].distance + lookupRoutingEntry(sourceMIP, receivedTable).distance;
        if (newDistance < routingTable[i].distance) {
            routingTable[i].distance = newDistance;
            routingTable[i].next_hop = sourceMIP;
        }
    }
}

/**
 * Check for timeouts in neighbor nodes and update the routing table accordingly.
 * 
 * route_fd: File descriptor used for sending routing updates.
 * 
 * This function iterates through the neighbor table and checks if any neighbor has 
 * exceeded the TIMEOUT_INTERVAL without sending a 'hello' message. For neighbors that 
 * have timed out, it sets their entry in the neighbor table to 0 (indicating they are 
 * no longer reachable), updates their status in the neighborStatus array, and modifies 
 * their corresponding entry in the routing table to reflect the loss of the route 
 * (setting next_hop to -1 and distance to INFINITY). After updating the routing table 
 * for a timed-out neighbor, it sends a routing update using the 'sendUpdateFromApp' function.
 * 
 * Note: The function relies on global arrays 'neighborTable', 'neighborStatus', and 'routingTable'.
 */
void checkForNeighborTimeouts(int route_fd) {
    time_t currentTime = time(NULL);
    for (int i = 0; i < MAX_NODES; i++) {
        if (neighborTable[i] && (currentTime - neighborStatus[i].lastHelloReceived > TIMEOUT_INTERVAL)) {
            neighborTable[i] = 0;
            neighborStatus[i].isReachable = 0;
            routingTable[i].next_hop = -1;
            routingTable[i].distance = INFINITY;
            
            // Send routing update
            sendUpdateFromApp(route_fd);
        }
    }
}

/**
 * Get the next hop MIP address for a given destination MIP.
 * 
 * destinationMIP: The MIP address of the destination node.
 * 
 * This function looks up the routing entry for the specified destination MIP address 
 * in the routing table. If the destination is valid (within the range of 0 to MAX_NODES - 1) 
 * and a route to it exists (distance is not INFINITY), it returns the next hop MIP address 
 * from the routing table entry. If the destination is out of range or if there is no known route 
 * (distance is INFINITY), the function returns 255, indicating an invalid or unreachable destination.
 * 
 * Returns the next hop MIP address for the destination, or 255 if the destination is invalid or unreachable.
 */
int getNextHopMIP(int destinationMIP) {
    if (destinationMIP >= 0 && destinationMIP < MAX_NODES) {
        struct RoutingEntry entry = lookupRoutingEntry(destinationMIP, routingTable);
        if (entry.distance != INFINITY) {
            return entry.next_hop;  // Return the next hop for the destination
        }
    }
    return 255;  // Return 255 if the destination is invalid or unreachable
}




/**
 * Process a received Hello message and update routing and neighbor tables.
 * 
 * MIPgreeter: The MIP address of the node that sent the Hello message.
 * 
 * This function handles the processing of a received Hello message. It first checks if 
 * the MIP address of the sender (MIPgreeter) is within a valid range (0 to MAX_NODES - 1). 
 * If so, it marks this MIP address as a neighbor in the neighbor table. Additionally, if 
 * this is a new neighbor (indicated by no existing route to it in the routing table), the 
 * function initializes a direct route to this neighbor in the routing table with a cost of 1.
 * 
 * Note: The function modifies global arrays 'neighborTable' and 'routingTable'.
 */
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

void handleUpdateMessage(uint8_t *updateMessage, int messageLength) {
    if (messageLength < 3 * MAX_NODES + 5) { // Check for minimum length (header + at least one entry)
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

/**
 * Process a received routing update message and update the local routing table.
 * 
 * updateMessage: Pointer to the buffer containing the update message.
 * messageLength: Length of the update message.
 * 
 * This function processes a routing update message received from another node. It first 
 * checks if the message has a valid length (header plus at least one routing entry). It 
 * extracts the sender's MIP address from the message and then iterates over each routing 
 * entry in the message. It performs a Poisoned Reverse check to ignore routes where this 
 * node is listed as the next hop. For valid entries, it calculates the new distance to 
 * each destination and updates the local routing table if the new distance is shorter 
 * than the existing one. The direct link cost to the sender is assumed to be 1.
 * 
 * Note: The function modifies the global 'routingTable' and assumes a 'localMIP' variable.
 */
void handleRequestMessage(int route_fd, uint8_t *requestMessage, int messageLength) {
    if (messageLength < 6) { // Check for minimum length (header + at least one entry)
        printf("Invalid request message length.\n");
        return;
    }

    uint8_t destinationMIP = requestMessage[5];
    // Assuming requestMessage[2], requestMessage[3], and requestMessage[4] are 'R', 'E', 'Q'
    
    uint8_t next_hop = getNextHopMIP(destinationMIP);
    sendResponseFromApp(route_fd, next_hop);
}

/**
 * Handle incoming messages on a routing file descriptor.
 * 
 * route_fd: File descriptor for reading routing messages.
 * 
 * This function reads messages from the specified file descriptor and determines the type 
 * of each message based on its contents. It handles three types of messages: 'hello', 
 * 'routing update', and 'request'. For each type, it calls the respective handler function 
 * ('handleHelloMessage', 'handleUpdateMessage', 'handleRequestMessage'). If the message type 
 * is unrecognized, it prints an error message and exits the program. The function also handles 
 * read errors by printing an error message and exiting.
 * 
 * Note: The function assumes the presence of handler functions for different message types 
 * and makes use of 'read_buf' for storing incoming message data.
 */
void handleIncomingMessages(int route_fd) {
    uint8_t read_buf[1024];
    int rc = read(route_fd, read_buf, sizeof(read_buf));
    if (rc < 0) {
        perror("read");
        close(route_fd);
        exit(EXIT_FAILURE);
    } else {
        printf("Received %d bytes.\n", rc);
    }

    if (read_buf[2] == 0x48 && read_buf[3] == 0x45 && read_buf[4] == 0x4C) {
        printf("Received hello message.\n");
        handleHelloMessage(read_buf[0]);
        
    } else if (read_buf[2] == 0x55 && read_buf[3] == 0x50 && read_buf[4] == 0x44) {
        printf("Received routing update.\n");
        handleUpdateMessage(read_buf, rc);
    } else if (read_buf[2] == 0x52 && read_buf[3] == 0x45 && read_buf[4] == 0x51) {
        printf("Received request message.\n");
        handleRequestMessage(route_fd, read_buf, rc);
    } else {
        printf("Invalid message type.\n");

        // Quit if the message type is invalid
        close(route_fd);
        exit(EXIT_FAILURE);

    }
}



/**
 * Send a Hello message from the application through the specified routing file descriptor.
 * 
 * route_fd: File descriptor used for sending the Hello message.
 * 
 * This function constructs and sends a Hello message using the specified file descriptor. 
 * The message includes the local MIP address and a TTL value set to zero, followed by 
 * ASCII values representing 'HEL'. The function handles sending errors by printing an error 
 * message. On successful sending, it prints a confirmation message.
 * 
 * Note: The function assumes the presence of a global variable 'localMIP' representing the 
 * local MIP address.
 */
void sendHelloFromApp(int route_fd) {
    uint8_t helloMessage[] = {
        localMIP, // MIP address
        0x00,           // TTL set to zero
        0x48,           // ASCII for 'H'
        0x45,           // ASCII for 'E'
        0x4C            // ASCII for 'L'
    };

    int bytes_sent = send(route_fd, helloMessage, sizeof(helloMessage), 0);
    if (bytes_sent < 0) {
        perror("send");
    } else {
        printf("Hello message sent.\n");
    }
}

/**
 * Send a routing update message from the application through the specified routing file descriptor.
 * 
 * route_fd: File descriptor used for sending the routing update message.
 * 
 * This function constructs a routing update message containing the local MIP address, a TTL value 
 * set to zero, and the routing table entries. Each entry in the routing table is added to the message 
 * if it has a valid next hop. The message is then sent using the specified file descriptor. The 
 * function handles sending errors by printing an error message. On successful sending, it prints a 
 * confirmation message.
 * 
 * Note: The function assumes the presence of a global variable 'localMIP' and a global 'routingTable' array.
 */
void sendUpdateFromApp(int route_fd) {
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

    int bytes_sent = send(route_fd, updateMessage, messageLength, 0);
    if (bytes_sent < 0) {
        perror("send");
    } else {
        printf("Routing update sent.\n");
    }
}


/**
 * Send a response message from the application through the specified routing file descriptor.
 * 
 * route_fd: File descriptor used for sending the response message.
 * next_hop: The next hop MIP address to be included in the response message.
 * 
 * This function constructs a response message including the local MIP address, a TTL value set 
 * to zero, ASCII values for 'RES', and the provided next hop MIP address. The message is then 
 * sent using the specified file descriptor. The function handles sending errors by printing an 
 * error message. On successful sending, it prints a confirmation message.
 * 
 * Note: The function assumes the presence of a global variable 'localMIP'.
 */
void sendResponseFromApp(int route_fd, int next_hop) {
    uint8_t responseMessage[] = {
        localMIP,       // MIP address
        0x00,           // TTL set to zero
        0x52,           // ASCII for 'R'
        0x45,           // ASCII for 'E'
        0x53,           // ASCII for 'S'
        next_hop        // Next hop for the destination
    };

    int bytes_sent = send(route_fd, responseMessage, sizeof(responseMessage), 0);
    if (bytes_sent < 0) {
        perror("send");
    } else {
        printf("Response message sent.\n");
    }
}


