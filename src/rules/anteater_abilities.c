#include <stdio.h>
#include <stdlib.h>

#include "anteater_abilities.h"
#include "cards.h"
#include "deck.h"
#include "poker_rules.h"

/*
 * Anteater abilities are intentionally server-side. Clients may request an
 * ability, but only the server can reveal cards, move turns, or change points.
 */

static int valid_player_seat(const GameState *game, int seat)
{
    return game != NULL &&
           seat >= 0 &&
           seat < MAX_PLAYERS &&
           game->players[seat].status != PLAYER_EMPTY;
}

int anteater_apply_ability(GameState *game, int seat, int target_seat,
                           int param, char *result, int result_size)
{
    if (result != NULL && result_size > 0)
    {
        result[0] = '\0';
    }

    if (!valid_player_seat(game, seat))
    {
        return 0;
    }

    Player *player = &game->players[seat];
    if (player->ability.type == ABILITY_NONE || player->ability.used)
    {
        snprintf(result, result_size, "No unused ability available.");
        return 0;
    }
    if (game->phase < PHASE_PREFLOP || game->phase > PHASE_RIVER)
    {
        snprintf(result, result_size, "Abilities can only be used during a hand.");
        return 0;
    }

    switch (player->ability.type)
    {
    case ABILITY_SNIFF:
    {
        /* Reveal one random private card from the chosen opponent. */
        if (!valid_player_seat(game, target_seat) || target_seat == seat)
        {
            snprintf(result, result_size, "Sniff needs another player target.");
            return 0;
        }
        int card_index = rand() % PRIVATE_HAND_SIZE;
        char card_text[64];
        card_to_string(game->players[target_seat].hand[card_index],
                       card_text, sizeof(card_text));
        player->ability.used = 1;
        snprintf(result, result_size, "Sniff revealed seat %d card %d: %s",
                 target_seat + 1, card_index + 1, card_text);
        return 1;
    }

    case ABILITY_ANT_TRAIL:
    {
        /*
         * Peek at the next unrevealed community card without dealing it.
         * param 0 reveals suit; param 1 reveals rank/number.
         */
        if (game->deck.top >= DECK_SIZE)
        {
            snprintf(result, result_size, "No deck card is available to trail.");
            return 0;
        }

        Card next_card = game->deck.cards[game->deck.top];
        player->ability.used = 1;
        if (param == 1)
        {
            snprintf(result, result_size, "Ant Trail found the next community rank: %s.",
                     rank_to_string(next_card.rank));
        }
        else
        {
            snprintf(result, result_size, "Ant Trail found the next community suit: %s.",
                     suit_to_string(next_card.suit));
        }
        return 1;
    }

    case ABILITY_POSE:
    {
        /* Cancel another player's unused special ability card. */
        if (!valid_player_seat(game, target_seat) || target_seat == seat)
        {
            snprintf(result, result_size, "Pose needs another player target.");
            return 0;
        }
        if (game->players[target_seat].ability.type == ABILITY_NONE ||
            game->players[target_seat].ability.used)
        {
            snprintf(result, result_size, "Seat %d has no unused ability to cancel.",
                     target_seat + 1);
            return 0;
        }
        game->players[target_seat].ability.used = 1;
        player->ability.used = 1;
        snprintf(result, result_size, "Pose canceled seat %d's ability.", target_seat + 1);
        return 1;
    }

    case ABILITY_LONG_TONGUE:
    {
        /* Swap one of your private cards with the top card of the deck. */
        int card_index = target_seat == 1 ? 1 : 0;
        Card old_card;
        Card new_card;

        if (game->deck.top >= DECK_SIZE)
        {
            snprintf(result, result_size, "No deck card is available for Long Tongue.");
            return 0;
        }

        old_card = player->hand[card_index];
        new_card = deal_card(&game->deck);
        player->hand[card_index] = new_card;
        player->ability.used = 1;

        char old_text[64];
        char new_text[64];
        card_to_string(old_card, old_text, sizeof(old_text));
        card_to_string(new_card, new_text, sizeof(new_text));
        snprintf(result, result_size, "Long Tongue swapped %s for %s.",
                 old_text, new_text);
        return 1;
    }

    case ABILITY_WILD_GRAB:
    {
        /*
         * Replace one private card with a wildcard. The rules engine treats
         * invalid_card in a private hand as any rank/suit during showdown.
         */
        int card_index = target_seat == 1 ? 1 : 0;
        Card old_card = player->hand[card_index];

        if (!is_valid_card(old_card))
        {
            snprintf(result, result_size, "That card is already a wildcard.");
            return 0;
        }

        player->hand[card_index] = create_card(RANK_INVALID, SUIT_INVALID);
        player->ability.used = 1;
        player->ability.param = card_index + 1;

        char old_text[64];
        card_to_string(old_card, old_text, sizeof(old_text));
        snprintf(result, result_size, "Wild Grab turned %s into a wildcard.",
                 old_text);
        return 1;
    }

    default:
        snprintf(result, result_size, "Unknown ability.");
        return 0;
    }
}
