CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

SRC_DIR = src
RULES_DIR = $(SRC_DIR)/rules
TEST_DIR = test
BIN_DIR = bin
BUILD_DIR = build

RULES_SRC = \
	$(RULES_DIR)/cards.c \
	$(RULES_DIR)/deck.c

RULES_OBJ = \
	$(BUILD_DIR)/cards.o \
	$(BUILD_DIR)/deck.o

TEST_DECK_SRC = $(TEST_DIR)/test_deck.c
TEST_DECK_BIN = $(BIN_DIR)/test_deck

.PHONY: all test clean directories

all: directories $(TEST_DECK_BIN)

directories:
	mkdir -p $(BIN_DIR)
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/cards.o: $(RULES_DIR)/cards.c include/cards.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/deck.o: $(RULES_DIR)/deck.c include/deck.h include/cards.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_DECK_BIN): $(TEST_DECK_SRC) $(RULES_OBJ)
	$(CC) $(CFLAGS) $(TEST_DECK_SRC) $(RULES_OBJ) -o $(TEST_DECK_BIN) $(LDFLAGS)

test: all
	./$(TEST_DECK_BIN)

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TEST_DECK_BIN)