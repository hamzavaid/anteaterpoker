#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "deck.h"

void init_deck(Deck *deck)
{
    if (deck == NULL) {
        return;
    }

    int index = 0;

    for (Suit suit = SUIT_HEARTS; suit <= SUIT_SPADES; suit++) {
        for (Rank rank = RANK_TWO; rank <= RANK_ACE; rank++) {
            deck->cards[index] = create_card(rank, suit);
            index++;
        }
    }

    deck->top = 0;
}

void shuffle_deck(Deck *deck)
{
    if (deck == NULL) {
        return;
    }

    /*
     * Seed the random number generator only once.
     * This prevents repeated calls from producing the same shuffle
     * if they happen quickly.
     */
    static int seeded = 0;

    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    for (int i = DECK_SIZE - 1; i > 0; i--) {
        int j = rand() % (i + 1);

        Card temp = deck->cards[i];
        deck->cards[i] = deck->cards[j];
        deck->cards[j] = temp;
    }

    deck->top = 0;
}

Card deal_card(Deck *deck)
{
    if (deck == NULL || deck->top >= DECK_SIZE) {
        return create_card(RANK_INVALID, SUIT_INVALID);
    }

    Card dealt_card = deck->cards[deck->top];
    deck->top++;

    return dealt_card;
}

int cards_remaining(const Deck *deck)
{
    if (deck == NULL) {
        return 0;
    }

    return DECK_SIZE - deck->top;
}

int is_deck_empty(const Deck *deck)
{
    if (deck == NULL) {
        return 1;
    }

    return deck->top >= DECK_SIZE;
}

void print_deck(const Deck *deck)
{
    if (deck == NULL) {
        printf("Deck is NULL.\n");
        return;
    }

    char buffer[64];

    for (int i = 0; i < DECK_SIZE; i++) {
        card_to_string(deck->cards[i], buffer, sizeof(buffer));

        if (i < deck->top) {
            printf("%2d: %s [DEALT]\n", i + 1, buffer);
        } else {
            printf("%2d: %s\n", i + 1, buffer);
        }
    }

    printf("Cards remaining: %d\n", cards_remaining(deck));
}