#include <stdio.h>
#include "bot.h"
#include "poker_rules.h"
#include <string.h>
#include <stdlib.h>

/*
 * Looks at the bot's current memory of the table.
 * Only CALLS or CHECKS.
 */
const char* bot_decide_action(const BotState *state) {
    // Safety check: if state is somehow NULL, just fold to avoid a crash.
    if (state == NULL) {
        return "FOLD";
    }

    // Compare what the bot has put in the pot vs. the current table minimum.
    // If the bot owes money to stay in the hand, it must CALL.
    if (state->current_bet < state->table_highest_bet) {
        printf("Bot Logic: I owe points (%d < %d). Decided to CALL.\n", 
               state->current_bet, state->table_highest_bet);
        return "CALL";
    } 
    // If the bot's bet matches the table's bet, it is free to CHECK.
    else {
        // If we currently have matched the table, consider raising based on hand
        int raise_amount = bot_calculate_raise(state);
        if (raise_amount > state->current_bet) {
            printf("Bot Logic: Bets matched but strong hand. Decided to RAISE to %d.\n", raise_amount);
            return "RAISE";
        }

        printf("Bot Logic: Bets are matched (%d == %d). Decided to CHECK.\n", 
               state->current_bet, state->table_highest_bet);
        return "CHECK";
    }
}

/*
 * bot_calculate_raise
 */
int bot_calculate_raise(const BotState *state) {
    //If NULL state, can't calculate hand strength, so don't raise
    if (state == NULL) return 0;


    // Build a 7-card array: 2 hole cards + community + invalid placeholders
    Card cards[7];
    // Initialize all cards to invalid first
    for (int i = 0; i < 7; ++i) {
        cards[i] = create_card(RANK_INVALID, SUIT_INVALID);
    }
    // Fill in the bot's hole cards and any community cards on the table
    cards[0] = state->my_cards[0];
    cards[1] = state->my_cards[1];
    for (int i = 0; i < state->community_count && i < 5; ++i) {
        cards[i + 2] = state->community_cards[i];
    }


    PokerHandValue val;
    poker_evaluate_hand(cards, &val);

    int base = state->table_highest_bet;
    int max_total = state->current_bet + state->points;
    int desired = base; // new total bet (not amount to add)

    //Chance tfor the bot randomly decide to make an aggressive move regardless of hand strength, to GAMBLE YAAYYYYY
    int chance = rand() % 100;
    if (chance < 1 && max_total > state->current_bet) {
        // 1% chance to go all-in regardless of hand strength
        desired = max_total;
        printf("Bot Logic: Random all-in triggered (%d%% chance).\n", chance);
    } else if (chance < 15 && max_total > state->current_bet) {
        // 15% chance to make an aggressive raise
        int extra = state->points / 2;
        if (extra < 40) {
            extra = 40;
        }
        desired = state->table_highest_bet + extra;
        if (desired > max_total) desired = max_total;
        printf("Bot Logic: Random aggressive raise triggered (%d%% chance).\n", chance);
    } else {
        // Normal hand-strength-based raise behavior
        switch (val.rank) {
            case HAND_RANK_STRAIGHT_FLUSH:
            case HAND_RANK_FOUR_OF_A_KIND:
                desired = base + (state->pot / 2 > 50 ? state->pot / 2 : 50);
                break;
            case HAND_RANK_FULL_HOUSE:
                desired = base + (state->pot / 3 > 30 ? state->pot / 3 : 30);
                break;
            case HAND_RANK_FLUSH:
            case HAND_RANK_STRAIGHT:
            case HAND_RANK_THREE_OF_A_KIND:
                desired = base + (state->pot / 4 > 15 ? state->pot / 4 : 15);
                break;
            case HAND_RANK_TWO_PAIR:
                desired = base + 10;
                break;
            case HAND_RANK_PAIR:
                desired = base + 5;
                break;
            case HAND_RANK_HIGH_CARD:
            default:
                return 0; // don't raise on weak hands
        }
    }

    // Ensure the desired total is at least one more than current table bet
    if (desired <= state->table_highest_bet) desired = state->table_highest_bet + 1;

    // Cap to the bot's stack
    if (desired > max_total) desired = max_total;

    // If desired ends up being no change, don't raise
    if (desired <= state->current_bet) return 0;

    return desired;
}


/*
 * bot_update_state
 *
 * Reads the raw string from the server and extracts the numbers
 * so the bot can make math-based decisions.
 */
static int parse_player_state(const char *player_state, int my_seat, int *out_points, int *out_current_bet)
{
    if (player_state == NULL || my_seat < 0) {
        return 0;
    }

    const char *start = player_state;
    char entry[128];

    while (*start != '\0') {
        const char *comma = strchr(start, ',');
        size_t len = comma ? (size_t)(comma - start) : strlen(start);
        if (len >= sizeof(entry)) {
            return 0;
        }

        memcpy(entry, start, len);
        entry[len] = '\0';

        int seat = -1;
        char name[64];
        int points = 0;
        int current_bet = 0;
        int status = -1;
        int matched = sscanf(entry, "%d|%63[^|]|%d|%d|%d",
                             &seat, name, &points, &current_bet, &status);
        if (matched == 5 && seat == my_seat) {
            if (out_points) *out_points = points;
            if (out_current_bet) *out_current_bet = current_bet;
            return 1;
        }

        if (comma == NULL) {
            break;
        }
        start = comma + 1;
    }

    return 0;
}

void bot_update_state(BotState *state, const char *server_message) {
    if (state == NULL || server_message == NULL) return;

    // 1. Find out what seat the server assigned us
    // Format: "SEAT:<seat_num>:Welcome <name>\n"
    if (strncmp(server_message, "SEAT", 4) == 0) {
        sscanf(server_message, "SEAT:%d:", &state->my_seat);
        printf("Bot assigned to seat %d\n", state->my_seat);
    }

    // 2. Parse STAT messages to see whose turn it is and the current bets
    // Format: "STAT:-1:phase=X;players=Y;pot=Z;sidepot=W;turn=B;...;player_state=..."
    if (strncmp(server_message, "STAT", 4) == 0) {
        // A. Check whose turn it is
        char *turn_ptr = strstr(server_message, "turn=");
        if (turn_ptr) {
            int current_turn = -1;
            sscanf(turn_ptr, "turn=%d", &current_turn);
            if (current_turn == state->my_seat && state->my_seat != -1) {
                state->is_my_turn = 1;
            }
        }

        // B. Get the highest bet on the table
        char *table_bet_ptr = strstr(server_message, "current_bet=");
        if (table_bet_ptr) {
            sscanf(table_bet_ptr, "current_bet=%d", &state->table_highest_bet);
        }

        // C. Track the total pot (optional, but good for future AI logic)
        char *pot_ptr = strstr(server_message, "pot=");
        if (pot_ptr) {
            sscanf(pot_ptr, "pot=%d", &state->pot);
        }

        // D. Parse our current stack and bet from the public player state summary.
        char *player_state_ptr = strstr(server_message, "player_state=");
        if (player_state_ptr) {
            player_state_ptr += strlen("player_state=");
            parse_player_state(player_state_ptr, state->my_seat, &state->points, &state->current_bet);
        }
    }

    // 3. Parse HAND messages to know our own stats
    // Format: "HAND:<seat>:<card1>,<card2>,ability=<ability>;ability_used=<0|1>;points=<points>"
    if (strncmp(server_message, "HAND", 4) == 0) {
        int seat = -1;
        char card1_text[128] = {0};
        char card2_text[128] = {0};
        char ability_text[64] = {0};
        int ability_used = 0;
        int points = 0;

        int matched = sscanf(
            server_message,
            "HAND:%d:%127[^,],%127[^,],ability=%63[^;];ability_used=%d;points=%d",
            &seat,
            card1_text,
            card2_text,
            ability_text,
            &ability_used,
            &points);

        if (matched == 6 && seat == state->my_seat) {
            state->points = points;

            Card card1, card2;
            if (string_to_card(card1_text, &card1)) {
                state->my_cards[0] = card1;
            }
            if (string_to_card(card2_text, &card2)) {
                state->my_cards[1] = card2;
            }
        }
    }
}
