#ifndef _IPC_H_
#define _IPC_H_

#include <stdint.h>
#include <stddef.h>
#include <sys/epoll.h>


int create_unix_sock(const char *);
int add_to_epoll_table(int efd, int fd);

#endif