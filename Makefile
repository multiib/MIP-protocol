CC = gcc
CFLAGS = -Wall -Wextra #-std=c99
LDFLAGS =

COMMON_SRC = common.c
MIPD_SRC = mipd.c
PING_SERVER_SRC = ping_server.c
PING_CLIENT_SRC = ping_client.c

COMMON_OBJ = $(COMMON_SRC:.c=.o)
MIPD_OBJ = $(MIPD_SRC:.c=.o)
PING_SERVER_OBJ = $(PING_SERVER_SRC:.c=.o)
PING_CLIENT_OBJ = $(PING_CLIENT_SRC:.c=.o)

MIPD_EXE = mipd
PING_SERVER_EXE = ping_server
PING_CLIENT_EXE = ping_client

all: $(MIPD_EXE) $(PING_SERVER_EXE) $(PING_CLIENT_EXE)

$(MIPD_EXE): $(COMMON_OBJ) $(MIPD_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(PING_SERVER_EXE): $(COMMON_OBJ) $(PING_SERVER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(PING_CLIENT_EXE): $(COMMON_OBJ) $(PING_CLIENT_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(COMMON_OBJ) $(MIPD_OBJ) $(PING_SERVER_OBJ) $(PING_CLIENT_OBJ) $(MIPD_EXE) $(PING_SERVER_EXE) $(PING_CLIENT_EXE)
