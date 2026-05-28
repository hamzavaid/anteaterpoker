#include <stdio.h>
#include "bot.h"
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
        printf("Bot Logic: Bets are matched (%d == %d). Decided to CHECK.\n", 
               state->current_bet, state->table_highest_bet);
        return "CHECK";
    }
}

/*
 * bot_calculate_raise
 *
 * Returns 0 for now
 */
int bot_calculate_raise(const BotState *state) {
    (void)state; // Ignore unused variable warning
    return 0; 
}


/*
 * bot_update_state
 *
 * Reads the raw string from the server and extracts the numbers
 * so the bot can make math-based decisions.
 */
void bot_update_state(BotState *state, const char *server_message) {
    if (state == NULL || server_message == NULL) return;

    // 1. Find out what seat the server assigned us
    // Format: "SEAT:<seat_num>:Welcome <name>\n"
    if (strncmp(server_message, "SEAT", 4) == 0) {
        sscanf(server_message, "SEAT:%d:", &state->my_seat);
        printf("Bot assigned to seat %d\n", state->my_seat);
    }

    // 2. Parse STAT messages to see whose turn it is and the current bets
    // Format: "STAT:-1:phase=X;players=Y;pot=Z;current_bet=A;turn=B..."
    if (strncmp(server_message, "STAT", 4) == 0) {
        
        // A. Check whose turn it is
        char *turn_ptr = strstr(server_message, "turn=");
        if (turn_ptr) {
            int current_turn = -1;
            sscanf(turn_ptr, "turn=%d", &current_turn);
            
            // If the turn matches our seat, flag it so the main loop can act
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
    }

    // 3. Parse HAND messages to know our own stats
    // Format: "HAND:<seat>:cards=X,Y;points=Z;bet=W"
    if (strncmp(server_message, "HAND", 4) == 0) {
        // Extract how much the bot itself has currently bet
        char *my_bet_ptr = strstr(server_message, "bet=");
        if (my_bet_ptr) {
            sscanf(my_bet_ptr, "bet=%d", &state->current_bet);
        }
    }
}
