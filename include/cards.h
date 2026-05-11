#ifndef CARDS_H
#define CARDS_H

#include <stdio.h>

/*
 * cards.h
 * Defines the basic Card structure, suits, ranks, and helper functions
 * for Anteater Poker.
*/

typedef enum {
    SUIT_HEARTS = 0,
    SUIT_DIAMONDS,
    SUIT_CLUBS,
    SUIT_SPADES,
    SUIT_INVALID
} Suit;

typedef enum {
    RANK_TWO = 2,
    RANK_THREE,
    RANK_FOUR,
    RANK_FIVE,
    RANK_SIX,
    RANK_SEVEN,
    RANK_EIGHT,
    RANK_NINE,
    RANK_TEN,
    RANK_JACK,
    RANK_QUEEN,
    RANK_KING,
    RANK_ACE,
    RANK_INVALID
} Rank;

typedef struct {
    Rank rank;
    Suit suit;
} Card;

Card create_card(Rank rank, Suit suit);
int is_valid_card(Card card);
int cards_equal(Card a, Card b);
const char *suit_to_string(Suit suit);
const char *rank_to_string(Rank rank);
void card_to_string(Card card, char *buffer, int buffer_size);
void print_card(Card card);

#endif