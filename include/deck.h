#ifndef DECK_H
#define DECK_H

#include "cards.h"

/*
 * deck.h
 * Handles creating, shuffling, and dealing a standard 52-card poker deck.
*/

#define DECK_SIZE 52

typedef struct {
    Card cards[DECK_SIZE];
    int top;
} Deck;

void init_deck(Deck *deck);
void shuffle_deck(Deck *deck);
Card deal_card(Deck *deck);
int cards_remaining(const Deck *deck);
int is_deck_empty(const Deck *deck);
void print_deck(const Deck *deck);

#endif