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


#define MAX_CONNS 3

/**
 * Create and initialize a UNIX domain socket.
 * 
 * socket_path: Path to the UNIX socket to be created.
 * This function creates a UNIX domain socket with SOCK_SEQPACKET type, binds it to 
 * the specified socket_path, and sets it up to listen for incoming connections. 
 * The maximum number of pending connections is set to MAX_CONNS.
 * 
 * Steps:
 * 1. Create a socket.
 * 2. Bind the socket to the provided socket path.
 * 3. Set the socket to listen for incoming connections.
 * 
 * Note: If any step fails, an error is printed and the function returns -1.
 * 
 * Returns the socket descriptor on success, or -1 on failure.
 */
int create_unix_sock(const char *socket_path)
{
    struct sockaddr_un addr;
    int sd = -1, rc = -1;

    sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sd == -1) {
        perror("socket()");
        return rc;
    }

    /* Clear the whole structure for portability */
    memset(&addr, 0, sizeof(struct sockaddr_un));

    /* Bind socket to socket name */
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    /* Unlink the socket for reuse */
    unlink(socket_path);

    rc = bind(sd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rc == -1) {
        perror("bind");
        close(sd);
        return rc;
    }

    /* Prepare for accepting incoming connections */
    rc = listen(sd, MAX_CONNS);
    if (rc == -1) {
        perror("listen()");
        close(sd);
        return rc;
    }

    return sd;

}

/**
 * Add a file descriptor to the epoll event table.
 * 
 * efd: Epoll instance file descriptor.
 * fd: File descriptor to be added to the epoll event table.
 * This function configures an epoll_event structure with the EPOLLIN event type
 * (indicating readiness to read) and the file descriptor to monitor. It then adds
 * this event to the epoll instance referred to by efd.
 * 
 * Note: In case of an error in adding the fd to the epoll table, an error message
 * is printed and -1 is returned.
 * 
 * Returns 0 on successful addition, -1 on failure.
 */
int add_to_epoll_table(int efd, int fd)
{
        int rc = 0;

        struct  epoll_event ev;
        
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                perror("epoll_ctl");
                rc = -1;
        }

        return rc;
}
