#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Declaration of the parse_arguments function
void parse_arguments(int argc, char *argv[], char **socket_lower, char **destination_host, char **message);

int main(int argc, char *argv[]) {
    char *socket_lower = NULL;
    char *destination_host = NULL;
    char *message = NULL;

    // Call the function to parse arguments
    parse_arguments(argc, argv, &socket_lower, &destination_host, &message);

    // Now you can use 'socket_lower', 'destination_host', and 'message' in your program

    // Example: Print the values
    printf("Socket lower: %s\n", socket_lower);
    printf("Destination host: %s\n", destination_host);
    printf("Message: %s\n", message);

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
