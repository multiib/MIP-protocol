#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MAX_CONNS 3

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
