# Makefile for Anteater Poker
#
# This Makefile builds:
#   1. bin/server              -> server executable
#   2. bin/poker               -> terminal client executable
#   3. bin/test_deck           -> deck unit test
#   4. bin/test_server_client  -> simple client used to test server login
#
# Main commands:
#   make        -> builds everything
#   make server -> builds only the server
#   make client -> builds only the client
#   make test   -> builds everything and runs the deck test
#   make clean  -> removes generated build files and executables

# Compiler
CC = gcc

# GTK settings.
# These commands ask Ubuntu where the GTK header files and libraries are.
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Compiler and linker flags.
# GTK_CFLAGS fixes: fatal error: gtk/gtk.h: No such file or directory
# GTK_LIBS fixes linker errors for GTK functions.
CFLAGS = -Wall -Wextra -std=c11 -Iinclude $(GTK_CFLAGS)
LDFLAGS = $(GTK_LIBS)

# Project directories
SRC_DIR = src
RULES_DIR = $(SRC_DIR)/rules
SERVER_DIR = $(SRC_DIR)/server
CLIENT_DIR = $(SRC_DIR)/client
TEST_DIR = test
BIN_DIR = bin
BUILD_DIR = build

# Executable output files
SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/poker
TEST_DECK_BIN = $(BIN_DIR)/test_deck
TEST_SERVER_CLIENT_BIN = $(BIN_DIR)/test_server_client

# Source files for the shared rules module
RULES_SRC = \
	$(RULES_DIR)/cards.c \
	$(RULES_DIR)/deck.c

# Source files for the server module
SERVER_SRC = \
	$(SERVER_DIR)/server.c \
	$(SERVER_DIR)/game_state.c \
	$(SERVER_DIR)/socket_server.c

# Source files for the client module
CLIENT_SRC = \
	$(CLIENT_DIR)/poker.c \
	$(CLIENT_DIR)/client_state.c \
	$(CLIENT_DIR)/socket_client.c

# Source files for tests
TEST_DECK_SRC = $(TEST_DIR)/test_deck.c
TEST_SERVER_CLIENT_SRC = $(TEST_DIR)/test_server_client.c

# Object files for the shared rules module
RULES_OBJ = \
	$(BUILD_DIR)/cards.o \
	$(BUILD_DIR)/deck.o

# Object files for the server module
SERVER_OBJ = \
	$(BUILD_DIR)/server.o \
	$(BUILD_DIR)/game_state.o \
	$(BUILD_DIR)/socket_server.o

# Object files for the client module
CLIENT_OBJ = \
	$(BUILD_DIR)/poker.o \
	$(BUILD_DIR)/client_state.o \
	$(BUILD_DIR)/socket_client.o

.PHONY: all test server client clean directories

# Default command: build everything.
all: directories $(SERVER_BIN) $(CLIENT_BIN) $(TEST_DECK_BIN) $(TEST_SERVER_CLIENT_BIN)

# Creates folders for executables and object files.
directories:
	mkdir -p $(BIN_DIR)
	mkdir -p $(BUILD_DIR)

# Builds only the server executable.
server: directories $(SERVER_BIN)

# Builds only the client executable.
client: directories $(CLIENT_BIN)

# General compile rule for files inside src/rules/.
$(BUILD_DIR)/%.o: $(RULES_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# General compile rule for files inside src/server/.
$(BUILD_DIR)/%.o: $(SERVER_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# General compile rule for files inside src/client/.
$(BUILD_DIR)/%.o: $(CLIENT_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Builds the server executable.
$(SERVER_BIN): $(SERVER_OBJ) $(RULES_OBJ) | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Builds the terminal client executable.
$(CLIENT_BIN): $(CLIENT_OBJ) $(RULES_OBJ) $(BUILD_DIR)/game_state.o | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Builds the deck test executable.
$(TEST_DECK_BIN): $(TEST_DECK_SRC) $(RULES_OBJ) | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Builds the server-client test executable.
$(TEST_SERVER_CLIENT_BIN): $(TEST_SERVER_CLIENT_SRC) | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Runs basic tests.
test: all
	./$(TEST_DECK_BIN)
	@echo ""
	@echo "Deck test finished."
	@echo ""
	@echo "To test the server manually:"
	@echo "  Terminal 1: ./bin/server --port 10010 --table \"ZotHouse\""
	@echo "  Terminal 2: ./bin/test_server_client"
	@echo ""
	@echo "To test the terminal client manually:"
	@echo "  Terminal 1: ./bin/server --port 10010 --table \"ZotHouse\""
	@echo "  Terminal 2: ./bin/poker --host localhost --port 10010 --name Hamza"

# Removes generated files.
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(SERVER_BIN)
	rm -f $(CLIENT_BIN)
	rm -f $(TEST_DECK_BIN)
	rm -f $(TEST_SERVER_CLIENT_BIN)