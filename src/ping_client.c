#include <stdio.h>		/* standard input/output library functions */
#include <stdlib.h>		/* standard library definitions (macros) */
#include <unistd.h>		/* standard symbolic constants and types */
#include <string.h>		/* string operations (strncpy, memset..) */
#include <time.h>        /* time functions */
#include <sys/epoll.h>	/* epoll */
#include <sys/socket.h>	/* sockets operations */
#include <sys/un.h>		/* definitions for UNIX domain sockets */
#include <sys/time.h>


#include "ipc.h"
#include "utils.h"
#include "pdu.h"


// In a source file (e.g., globals.c)
int app_fd;
int route_fd;


// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower, char **destination_host, char **message, char **ttl);


int main(int argc, char *argv[]) {
    char *socket_lower = NULL;
    char *destination_host = NULL;
    char *message = NULL;
    char *ttl = NULL;

    int sd, rc;
    int epfd, nfds;
    struct epoll_event event, events[1];  // We'll just wait for one event





    // Call the function to parse arguments
    parse_arguments(argc, argv, &socket_lower, &destination_host, &message, &ttl);

    
    struct sockaddr_un addr;
    char   buf[512];
    uint32_t read_buf[128];

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

    // Write identifier to socket
    uint8_t identifier = 0x01;
    rc = write(sd, &identifier, 1);
    if (rc < 0) {
        perror("write");
        close(sd);
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to %s\n", socket_lower);


    printf("Sending message to %s with TTL %s\n", destination_host, ttl);

    fill_ping_buf(buf, sizeof(buf), destination_host, message, ttl);






    // Create an epoll instance
    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add the socket descriptor to the epoll instance
    event.events = EPOLLIN;  // We're interested in input events
    event.data.fd = sd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sd, &event) == -1) {
        perror("epoll_ctl: EPOLL_CTL_ADD");
        close(epfd);
        exit(EXIT_FAILURE);
    }


    struct timeval start, end;
    long mtime, seconds, useconds;
    
    gettimeofday(&start, NULL);

    // Write to socket
    rc = write(sd, buf, strlen(buf));
    if (rc < 0) {
        perror("write");
        close(epfd);
        exit(EXIT_FAILURE);
    }

    // Wait for events on the epoll file descriptor
    nfds = epoll_wait(epfd, events, 1, 5000);  // Timeout after 5000 milliseconds
    if (nfds == -1) {
        perror("epoll_wait");
        close(epfd);
        exit(EXIT_FAILURE);
    } else if (nfds == 0) {
        printf("Timeout occurred! No data received for 5 seconds.\n");
        close(epfd);
        exit(EXIT_FAILURE);
    }

    // Read from socket
    rc = read(sd, read_buf, sizeof(read_buf));
    if (rc < 0) {
        perror("read");
        close(epfd);
        exit(EXIT_FAILURE);
    }

    // Stop the clock
    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;
    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

    // Check if the elapsed time has passed a certain threshold
    if (mtime > 5000.0) { // Assume a 5-second threshold
        printf("Operation took too long: %ld milliseconds.\n", mtime);
    } else {
        printf("Operation completed in time: %ld milliseconds.\n", mtime);
    }
    char *str = uint32ArrayToString(read_buf);
    printf("%s\n", str);


    close(sd);
    return 0;
}

// Definition of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower, char **destination_host, char **message, char **ttl) {
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] <socket_lower> <destination_host> <message> <ttl>\n", argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Usage: %s [-h] <socket_lower> <destination_host> <message> <ttl>\n", argv[0]);
                exit(1);
        }
    }

    if (optind + 4 != argc) {
        fprintf(stderr, "Usage: %s [-h] <socket_lower> <destination_host> <message> <ttl>\n", argv[0]);
        exit(1);
    }

    *socket_lower = argv[optind];
    *destination_host = argv[optind + 1];
    *message = argv[optind + 2];
    *ttl = argv[optind + 3];
}