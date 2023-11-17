#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/un.h>		/* definitions for UNIX domain sockets */

#include "arp.h"
#include "ether.h"
#include "pdu.h"
#include "utils.h"
#include "mip.h"
#include "ipc.h"
#include "route.h"


#define HELLO_INTERVAL 10    // Interval in seconds for sending hello messages
#define TIMEOUT_INTERVAL 30 // Seconds



NeighborStatus neighborStatus[MAX_NODES];


uint8_t localMIP;  // Declare this as a global variable

RoutingEntry routingTable[MAX_NODES];

int routingTableHasChanged = 0; // Global flag for routing table chan



// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_lower);



int main(int argc, char *argv[]) {
    int debug_mode = 0;
    char *socket_lower = NULL;

    parse_arguments(argc, argv, &debug_mode, &socket_lower);

    RoutingEntry routingTable[MAX_NODES];
    int neighborTable[MAX_NODES]; // 1 indicates a neighbor, 0 otherwise


    // Set up the UNIX domain socket
    int sd, rc;



    struct sockaddr_un addr;
    sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_lower, sizeof(addr.sun_path) - 1);

    rc = connect(sd, (struct sockaddr *)&addr, sizeof(addr));
    if ( rc < 0) {
            perror("connect");
            close(sd);
            exit(EXIT_FAILURE);
    }

    printf("Connected to %s\n", socket_lower);


    // Read the MIP address from the socket
    uint8_t localMIP;
    if (read(sd, &localMIP, 1) < 0) {
        perror("read");
        close(sd);
        exit(EXIT_FAILURE);
    }
    printf("Received MIP address: %u\n", localMIP);

    pthread_t send_thread, receive_thread;

    // Create threads
    if (pthread_create(&send_thread, NULL, sendMessagesThread, &sd) != 0 ||
        pthread_create(&receive_thread, NULL, receiveMessagesThread, &sd) != 0) {
        perror("pthread_create");
        close(sd);
        exit(EXIT_FAILURE);
    }

    // Join threads or handle as needed
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sd);
    return 0;
}

void *sendMessagesThread(void *arg) {
    int socket_fd = *((int *)arg);
    while (1) {
        sendHelloMessage(socket_fd, localMIP);
        if (routingTableHasChanged) {
            sendRoutingUpdate(socket_fd, routingTable);
            routingTableHasChanged = 0;
        }
        checkForNeighborTimeouts(socket_fd, localMIP);
        sleep(HELLO_INTERVAL);
    }
    return NULL;
}

void *receiveMessagesThread(void *arg) {
    int socket_fd = *((int *)arg);
    while (1) {
        handleIncomingMessages(socket_fd);
    }
    return NULL;
}

// Implementation for sendHelloMessage, handleIncomingMessages, sendRoutingUpdate...

void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_lower) {
    int opt;
    *debug_mode = 0;

    while ((opt = getopt(argc, argv, "hd:")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] [-d <socket_path>]\n", argv[0]);
                exit(0);
            case 'd':
                *debug_mode = 1;
                *socket_lower = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-h] [-d <socket_path>]\n", argv[0]);
                exit(1);
        }
    }
}