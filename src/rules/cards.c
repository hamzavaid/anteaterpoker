#include <stdio.h>
#include <string.h>
#include "cards.h"

Card create_card(Rank rank, Suit suit)
{
    Card card;
    card.rank = rank;
    card.suit = suit;
    return card;
}

int is_valid_card(Card card)
{
    if (card.rank < RANK_TWO || card.rank > RANK_ACE) {
        return 0;
    }

    if (card.suit < SUIT_HEARTS || card.suit > SUIT_SPADES) {
        return 0;
    }

    return 1;
}

int cards_equal(Card a, Card b)
{
    return a.rank == b.rank && a.suit == b.suit;
}

const char *suit_to_string(Suit suit)
{
    switch (suit) {
        case SUIT_HEARTS:
            return "Hearts";
        case SUIT_DIAMONDS:
            return "Diamonds";
        case SUIT_CLUBS:
            return "Clubs";
        case SUIT_SPADES:
            return "Spades";
        default:
            return "Invalid Suit";
    }
}

const char *rank_to_string(Rank rank)
{
    switch (rank) {
        case RANK_TWO:
            return "Two";
        case RANK_THREE:
            return "Three";
        case RANK_FOUR:
            return "Four";
        case RANK_FIVE:
            return "Five";
        case RANK_SIX:
            return "Six";
        case RANK_SEVEN:
            return "Seven";
        case RANK_EIGHT:
            return "Eight";
        case RANK_NINE:
            return "Nine";
        case RANK_TEN:
            return "Ten";
        case RANK_JACK:
            return "Jack";
        case RANK_QUEEN:
            return "Queen";
        case RANK_KING:
            return "King";
        case RANK_ACE:
            return "Ace";
        default:
            return "Invalid Rank";
    }
}

void card_to_string(Card card, char *buffer, int buffer_size)
{
    if (buffer == NULL || buffer_size <= 0) {
        return;
    }

    if (!is_valid_card(card)) {
        snprintf(buffer, buffer_size, "Invalid Card");
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%s of %s",
        rank_to_string(card.rank),
        suit_to_string(card.suit)
    );
}

void print_card(Card card)
{
    char buffer[64];

    card_to_string(card, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}