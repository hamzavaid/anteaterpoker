# Makefile for Anteater Poker
#
# Required rubric commands:
#   make
#   make clean
#   make test
#   make test-gui
#   make tar
#
# Main executables:
#   bin/poker_server
#   bin/poker_client
#
# Legacy symlinks:
#   bin/server -> poker_server
#   bin/poker  -> poker_client

CC = gcc

# GTK settings for Ubuntu/Linux.
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0)

CFLAGS = -Wall -Wextra -std=c11 -g -Iinclude $(GTK_CFLAGS)
LDFLAGS = $(GTK_LIBS)

# Directories
SRC_DIR = src
RULES_DIR = $(SRC_DIR)/rules
SERVER_DIR = $(SRC_DIR)/server
CLIENT_DIR = $(SRC_DIR)/client
BOT_DIR = $(SRC_DIR)/bot
GUI_DIR = $(SRC_DIR)/gui
TEST_DIR = test
BIN_DIR = bin
BUILD_DIR = build
DIST_DIR = dist

# Output programs required by alpha rubric
SERVER_BIN = $(BIN_DIR)/poker_server
CLIENT_BIN = $(BIN_DIR)/poker_client

# Optional legacy names
LEGACY_SERVER_BIN = $(BIN_DIR)/server
LEGACY_CLIENT_BIN = $(BIN_DIR)/poker

# Test binaries
TEST_DECK_BIN = $(BIN_DIR)/test_deck
TEST_SERVER_CLIENT_BIN = $(BIN_DIR)/test_server_client

# Archive names required by alpha rubric
USER_ARCHIVE = Poker_Alpha.tar.gz
SRC_ARCHIVE = Poker_Alpha_src.tar.gz

# Shared rule objects
RULES_OBJ = \
	$(BUILD_DIR)/cards.o \
	$(BUILD_DIR)/deck.o \
	$(BUILD_DIR)/anteater_abilities.o \
	$(BUILD_DIR)/poker_rules.o

# Server objects
SERVER_OBJ = \
    $(BUILD_DIR)/server.o \
    $(BUILD_DIR)/game_state.o \
    $(BUILD_DIR)/socket_server.o \
    $(BUILD_DIR)/server_gui.o \
    $(BUILD_DIR)/bot_logic.o

# Client objects
CLIENT_OBJ = \
    $(BUILD_DIR)/poker.o \
    $(BUILD_DIR)/client_state.o \
    $(BUILD_DIR)/socket_client.o \
    $(BUILD_DIR)/client_gui.o


.PHONY: all server client bot test test-gui clean directories tar source-tar user-tar legacy-links check-docs

# Build everything required.
all: directories $(SERVER_BIN) $(CLIENT_BIN) $(TEST_DECK_BIN) $(TEST_SERVER_CLIENT_BIN) legacy-links

directories:
	mkdir -p $(BIN_DIR)
	mkdir -p $(BUILD_DIR)

server: directories $(SERVER_BIN)

client: directories $(CLIENT_BIN)

bot: directories $(BOT_BIN)

# Compile files in src/rules/
$(BUILD_DIR)/%.o: $(RULES_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Compile files in src/server/
$(BUILD_DIR)/%.o: $(SERVER_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Compile files in src/client/
$(BUILD_DIR)/%.o: $(CLIENT_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Compile files in src/bot/
$(BUILD_DIR)/%.o: $(BOT_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Compile files in src/gui/
$(BUILD_DIR)/%.o: $(GUI_DIR)/%.c | directories
	$(CC) $(CFLAGS) -c $< -o $@

# Build server executable.
$(SERVER_BIN): $(SERVER_OBJ) $(RULES_OBJ) | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build client executable.
$(CLIENT_BIN): $(CLIENT_OBJ) $(RULES_OBJ) $(BUILD_DIR)/game_state.o | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build deck unit test.
$(TEST_DECK_BIN): $(TEST_DIR)/test_deck.c $(RULES_OBJ) | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build server-client communication test.
$(TEST_SERVER_CLIENT_BIN): $(TEST_DIR)/test_server_client.c | directories
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Legacy executable names.
legacy-links: $(SERVER_BIN) $(CLIENT_BIN)
	ln -sf poker_server $(LEGACY_SERVER_BIN)
	ln -sf poker_client $(LEGACY_CLIENT_BIN)

# Required by rubric: make test should succeed cleanly.
test: all
	./$(TEST_DECK_BIN)
	@echo ""
	@echo "Deck unit test finished."
	@echo ""
	@echo "Server-client test binary was built:"
	@test -x $(TEST_SERVER_CLIENT_BIN)
	@echo "  $(TEST_SERVER_CLIENT_BIN)"
	@echo ""
	@echo "Manual communication test:"
	@echo "  Terminal 1: ./bin/poker_server --port 10010 --table \"ZotHouse\" &"
	@echo "  Terminal 2: ./bin/test_server_client &"
	@echo ""
	@echo "Manual client GUI test:"
	@echo "  Terminal 1: ./bin/poker_server --port 10010 --table \"ZotHouse\" &"
	@echo "  Terminal 2: ./bin/poker_client --host localhost --port 10010 --name Hamza &"
	./$(SERVER_BIN) --port 10010 --table "ZotHouse" &
	./$(CLIENT_BIN) --host localhost --port 10010 --name Test_Player &

# Required by rubric: make test-gui should exist.
test-gui: all
	@test -x $(SERVER_BIN)
	@test -x $(CLIENT_BIN)
	@echo "GUI binaries built successfully."
	@echo ""
	@echo "Run this server GUI test:"
	@echo "  ./bin/poker_server --port 10010 --table \"ZotHouse\""
	@echo ""
	@echo "Then run this client GUI test in another terminal:"
	@echo "  ./bin/poker_client --host localhost --port 10010 --name Player &"
	./$(SERVER_BIN) --port 10010 --table "ZotHouse" &
	./$(CLIENT_BIN) --host localhost --port 10010 --name Test_Player &

# Check required documentation before packaging.
check-docs:
	@test -f README.md || (echo "Missing README"; exit 1)
	@test -f INSTALL.md || (echo "Missing INSTALL"; exit 1)
	@test -f COPYRIGHT.md || (echo "Missing COPYRIGHT"; exit 1)
	@test -f doc/Poker_UserManual.pdf || (echo "Missing doc/Poker_UserManual.pdf"; exit 1)
	@test -f doc/Poker_SoftwareSpec.pdf || (echo "Missing doc/Poker_SoftwareSpec.pdf"; exit 1)

# Required by rubric: make tar creates packages.
tar: source-tar user-tar
	@echo ""
	@echo "Created:"
	@echo "  $(USER_ARCHIVE)"
	@echo "  $(SRC_ARCHIVE)"

# Source-code package.
source-tar: check-docs
	rm -rf $(DIST_DIR)/Poker_Alpha_src
	mkdir -p $(DIST_DIR)/Poker_Alpha_src
	mkdir -p $(DIST_DIR)/Poker_Alpha_src/bin
	cp README.md INSTALL.md COPYRIGHT.md Makefile $(DIST_DIR)/Poker_Alpha_src/
	cp -R include src test doc $(DIST_DIR)/Poker_Alpha_src/
	tar -czf $(SRC_ARCHIVE) -C $(DIST_DIR) Poker_Alpha_src
	rm -rf $(DIST_DIR)/Poker_Alpha_src
	@echo "Created $(SRC_ARCHIVE)"

# User/customer binary package.
user-tar: all check-docs
	rm -rf $(DIST_DIR)/Poker_Alpha
	mkdir -p $(DIST_DIR)/Poker_Alpha/bin
	mkdir -p $(DIST_DIR)/Poker_Alpha/doc
	cp README.md $(DIST_DIR)/Poker_Alpha/README.md
	cp INSTALL.md $(DIST_DIR)/Poker_Alpha/INSTALL.md
	cp COPYRIGHT.md $(DIST_DIR)/Poker_Alpha/COPYRIGHT.md
	cp $(SERVER_BIN) $(DIST_DIR)/Poker_Alpha/bin/poker_server
	cp $(CLIENT_BIN) $(DIST_DIR)/Poker_Alpha/bin/poker_client
	cp doc/Poker_UserManual.pdf $(DIST_DIR)/Poker_Alpha/doc/Poker_UserManual.pdf

	# Copy assets if your GUI loads images from src/assets.
	if [ -d src/assets ]; then \
		mkdir -p $(DIST_DIR)/Poker_Alpha/src; \
		cp -R src/assets $(DIST_DIR)/Poker_Alpha/src/assets; \
	fi

	tar -czf $(USER_ARCHIVE) -C $(DIST_DIR) Poker_Alpha
	rm -rf $(DIST_DIR)/Poker_Alpha
	@echo "Created $(USER_ARCHIVE)"

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(DIST_DIR)
	rm -f $(SERVER_BIN)
	rm -f $(CLIENT_BIN)
	rm -f $(LEGACY_SERVER_BIN)
	rm -f $(LEGACY_CLIENT_BIN)
	rm -f $(TEST_DECK_BIN)
	rm -f $(TEST_SERVER_CLIENT_BIN)
	rm -f $(USER_ARCHIVE)
	rm -f $(SRC_ARCHIVE)
