#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ipc.h"
#include "utils.h"
#include "pdu.h"
// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower);

int main(int argc, char *argv[]) {
    char *socket_lower = NULL;

    // Call the function to parse arguments
    parse_arguments(argc, argv, &socket_lower);

    // Now you can use 'socket_lower' in your program

    // Example: Print the value
    printf("Socket lower: %s\n", socket_lower);

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
