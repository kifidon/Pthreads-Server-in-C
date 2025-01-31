# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Directories for header files and source files
INCLUDES = -I./
SRC_DIR = .
OBJ_DIR = ./obj

# Source and object files for the server
SERVER_SRC = $(SRC_DIR)/server.c
SERVER_OBJ = $(OBJ_DIR)/server.o

# Source and object files for the client
CLIENT_SRC = $(SRC_DIR)/client.c
CLIENT_OBJ = $(OBJ_DIR)/client.o

# Source and object files for the attacker
ATTACKER_SRC = $(SRC_DIR)/attacker.c
ATTACKER_OBJ = $(OBJ_DIR)/attacker.o

# Output binaries
SERVER_OUT = main
CLIENT_OUT = client
ATTACKER_OUT = attacker

# Default target
all: $(SERVER_OUT) $(CLIENT_OUT) $(ATTACKER_OUT)

# Linking the server executable
$(SERVER_OUT): $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $(SERVER_OUT)

# Linking the client executable
$(CLIENT_OUT): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $(CLIENT_OUT)

# Linking the attacker executable
$(ATTACKER_OUT): $(ATTACKER_OBJ)
	$(CC) $(ATTACKER_OBJ) -o $(ATTACKER_OUT) -lm  # Add -lm to link the math library

# Compile object files for all programs
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)  # Ensure obj directory exists before compiling
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(SERVER_OUT) $(CLIENT_OUT) $(ATTACKER_OUT) server_output_time_aggregated

ARGS ?= 100
# Run the server
runserver: $(SERVER_OUT)
	./$(SERVER_OUT) $(ARGS) 127.0.0.1 8000

# Run the client
runclient: $(CLIENT_OUT)
	./$(CLIENT_OUT) $(ARGS) 127.0.0.1 8000

# # Run the attacker
runattacker: $(ATTACKER_OUT)
	./$(ATTACKER_OUT) $(ARGS) 127.0.0.1 8000