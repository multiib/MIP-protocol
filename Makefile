# Compiler to use
CC = gcc

# Compiler flags
CFLAGS = -Wall -I./include

# Source directory
SRC_DIR = ./src

# Object directory
OBJ_DIR = ./obj

# Executable directory
BIN_DIR = ./bin

# Source files
SRC_FILES = arp.c mipd.c ping_client.c ping_server.c utils.c pdu.c

# Object files
OBJ_FILES = $(SRC_FILES:%.c=$(OBJ_DIR)/%.o)

# Executables
EXE_FILES = mipd ping_client ping_server

# Executable paths
EXE_PATHS = $(EXE_FILES:%=$(BIN_DIR)/%)

all: directories $(EXE_PATHS)

# Rule to make directories
directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

# General rule for making object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for making mipd executable
$(BIN_DIR)/mipd: $(OBJ_DIR)/mipd.o $(OBJ_DIR)/arp.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/pdu.o
	$(CC) $(CFLAGS) $^ -o $@

# Rule for making ping_client executable
$(BIN_DIR)/ping_client: $(OBJ_DIR)/ping_client.o $(OBJ_DIR)/utils.o
	$(CC) $(CFLAGS) $^ -o $@

# Rule for making ping_server executable
$(BIN_DIR)/ping_server: $(OBJ_DIR)/ping_server.o $(OBJ_DIR)/utils.o
	$(CC) $(CFLAGS) $^ -o $@

# Rule for cleaning the project
clean:
	rm -f $(OBJ_DIR)/*.o $(EXE_PATHS)

.PHONY: all directories clean
