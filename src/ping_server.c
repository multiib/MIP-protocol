#include <stdio.h>		/* standard input/output library functions */
#include <stdlib.h>		/* standard library definitions (macros) */
#include <unistd.h>		/* standard symbolic constants and types */
#include <string.h>		/* string operations (strncpy, memset..) */

#include <sys/epoll.h>	/* epoll */
#include <sys/socket.h>	/* sockets operations */
#include <sys/un.h>		/* definitions for UNIX domain sockets */
#include "ipc.h"
#include "utils.h"
#include "pdu.h"


// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower);

int main(int argc, char *argv[]) {
    char *socket_lower = NULL;

    int sd, rc;

    // Call the function to parse arguments
    parse_arguments(argc, argv, &socket_lower);

    struct sockaddr_un addr;
    char   buf[512];
    uint32_t read_buf[256];

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
    if (debug_mode){
        printf("Connected to %s\n", socket_lower);
    }

    while(1){
        // Read from socket
        rc = read(sd, read_buf, sizeof(read_buf));
        if (rc < 0) {
            perror("read");
            close(sd);
            exit(EXIT_FAILURE);
        }
        printf("Received %d bytes\n", rc);

        char *str = uint32ArrayToString(read_buf);
        printf("%s\n", str);


        printf("%d\n", read_buf[0]);
        char *destination_host = "11"; // Filler value
        
        // Fill the buffer with the pong message
        fill_pong_buf(buf, sizeof(buf), destination_host, strcpy(str, str + 5));
        free(str);
        str = NULL;
        // Write back
        rc = write(sd, buf, sizeof(buf));
        if (rc < 0) {
            perror("write");
            close(sd);
            exit(EXIT_FAILURE);
        }
    }


    close(sd);
    return 0;
}

// Definition of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower) {
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] <socket_lower>\n", argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Usage: %s [-h] <socket_lower>\n", argv[0]);
                exit(1);
        }
    }

    // After processing options, optind points to the first non-option argument
    if (optind + 1 != argc) {
        fprintf(stderr, "Usage: %s [-h] <socket_lower>\n", argv[0]);
        exit(1);
    }

    *socket_lower = argv[optind];
}

