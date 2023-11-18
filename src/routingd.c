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
#include <sys/un.h>      /* definitions for UNIX domain sockets */

#include "arp.h"
#include "ether.h"
#include "pdu.h"
#include "utils.h"
#include "mip.h"
#include "ipc.h"
#include "route.h"

#define HELLO_INTERVAL 10    // Interval in seconds for sending hello messages
#define TIMEOUT_INTERVAL 30  // Seconds

struct NeighborStatus neighborStatus[MAX_NODES];
uint8_t localMIP;  // Global variable for local MIP
struct RoutingEntry routingTable[MAX_NODES];
int routingTableHasChanged = 0;  // Global flag for routing table change
int neighborTable[MAX_NODES];     // 1 indicates a neighbor, 0 otherwise


// Function prototypes
void *sendMessagesThread(void *arg);
void *receiveMessagesThread(void *arg);
void parse_arguments(int argc, char *argv[], int *debug_mode, char **socket_lower);

int main(int argc, char *argv[]) {
    int debug_mode = 0;
    char *socket_lower = NULL;
    parse_arguments(argc, argv, &debug_mode, &socket_lower);

    // Set up the UNIX domain socket
    int rc;
    struct sockaddr_un addr;
    int route_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (route_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_lower, sizeof(addr.sun_path) - 1);

    rc = connect(route_fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0) {
        perror("connect");
        close(route_fd);
        exit(EXIT_FAILURE);
    }

    // Write identifier to socket
    uint8_t identifier = 0x02;
    rc = write(route_fd, &identifier, 1);
    if (rc < 0) {
        perror("write");
        close(route_fd);
        exit(EXIT_FAILURE);
    }
    

    printf("Connected to %s\n", socket_lower);

    // Read the MIP address from the socket
    if (read(route_fd, &localMIP, 1) < 0) {
        perror("read");
        close(route_fd);
        exit(EXIT_FAILURE);
    }
    printf("Received MIP address: %u\n", localMIP);

    pthread_t send_thread, receive_thread;

    // Create threads
    if (pthread_create(&send_thread, NULL, sendMessagesThread, &route_fd) != 0 ||
        pthread_create(&receive_thread, NULL, receiveMessagesThread, &route_fd) != 0) {
        perror("pthread_create");
        close(route_fd);
        exit(EXIT_FAILURE);
    }

    // Join threads or handle as needed
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(route_fd);
    return 0;
}

void *sendMessagesThread(void *arg) {
    int route_fd = *((int *)arg);
    while (1) {
        sendHelloFromApp(route_fd);
        if (routingTableHasChanged) {
            sendUpdateFromApp(route_fd);
            routingTableHasChanged = 0;
        }
        checkForNeighborTimeouts(route_fd);
        sleep(HELLO_INTERVAL);
    }
    return NULL;
}

void *receiveMessagesThread(void *arg) {
    int route_fd = *((int *)arg);
    while (1) {
        handleIncomingMessages(route_fd);
    }
    return NULL;
}

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

