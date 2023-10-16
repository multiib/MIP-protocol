#include <stdio.h>		/* standard input/output library functions */
#include <stdlib.h>		/* standard library definitions (macros) */
#include <unistd.h>		/* standard symbolic constants and types */
#include <string.h>		/* string operations (strncpy, memset..) */
#include <time.h>        /* time functions */
#include <sys/epoll.h>	/* epoll */
#include <sys/socket.h>	/* sockets operations */
#include <sys/un.h>		/* definitions for UNIX domain sockets */
#include <signal.h>
#include <setjmp.h>

#include "ipc.h"
#include "utils.h"
#include "pdu.h"

void sigalrm_handler(int signum);

volatile sig_atomic_t timed_out = 0;
jmp_buf env;



// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower, char **destination_host, char **message);

int main(int argc, char *argv[]) {
    char *socket_lower = NULL;
    char *destination_host = NULL;
    char *message = NULL;

    int sd, rc;

    clock_t start, end;
    double cpu_time_used;

    // Call the function to parse arguments
    parse_arguments(argc, argv, &socket_lower, &destination_host, &message);

    
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
    printf("Connected to %s\n", socket_lower);




    fill_ping_buf(buf, sizeof(buf), destination_host, message);

    signal(SIGALRM, sigalrm_handler);
    alarm(1);
    start = clock();
    rc = write(sd, buf, strlen(buf));
    if (rc < 0) {
            perror("write");
            close(sd);
            exit(EXIT_FAILURE);
    }






    if (setjmp(env) == 0) {


        // Read from socket
        rc = read(sd, read_buf, sizeof(read_buf));
        if (rc < 0) {
            perror("read");
            close(sd);
            exit(EXIT_FAILURE);
        }

    } else {
        printf("Operation timed out!\n");
    }









    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC * 1000; // Convert to milliseconds
    char *str = uint32ArrayToString(read_buf);
    printf("%s\n", str);
    printf("time=%f ms\n", cpu_time_used);

    close(sd);
    return 0;
}

// Definition of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower, char **destination_host, char **message) {
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: %s [-h] <socket_lower> <destination_host> <message>\n", argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Usage: %s [-h] <socket_lower> <destination_host> <message>\n", argv[0]);
                exit(1);
        }
    }

    // After processing options, optind points to the first non-option argument
    if (optind + 3 != argc) {
        fprintf(stderr, "Usage: %s [-h] <socket_lower> <destination_host> <message>\n", argv[0]);
        exit(1);
    }

    *socket_lower = argv[optind];
    *destination_host = argv[optind + 1];
    *message = argv[optind + 2];
}


void sigalrm_handler(int signum) {
    timed_out = 1;
    longjmp(env, 1);
}