#include <stdio.h>
#include "deck.h"

/*
Test for deck and card func
*/
int main(void)
{
    Deck deck;
    init_deck(&deck);
    shuffle_deck(&deck);

    printf("Dealing 5 cards:\n");

    for (int i = 0; i < 5; i++) {
        Card card = deal_card(&deck);
        print_card(card);
    }

    printf("Cards left: %d\n", cards_remaining(&deck));

    return 0;
}