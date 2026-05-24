#include <stdio.h>
#include <string.h>

#include "poker_rules.h"
#include "game_state.h"
#include "cards.h"

int poker_action_is_legal(const GameState *game,int seat, const char *action, int raise_amount){
    //Check if the game state and action are valid, and if it's the player's turn.
    if (game == NULL || action == NULL) {
        return 0;
    }

    //Check if the seat number is valid and corresponds to an active player.
    if (seat < 0 || seat >= game->player_count) {
        return 0;
    }

    //Check if it's the player's turn.
    if (game->current_turn != seat) {
        return 0;
    }

    const Player *player = &game->players[seat];

    
    if (strcmp(action, "FOLD") == 0) {
        return 1;
        //CHECK: If the player has already matched the current bet, they can check.
    } else if (strcmp(action, "CHECK") == 0) {
        return player->current_bet >= game->current_bet;
        //CALL: If the player has not yet matched the current bet, they can call if they have enough points.
    } else if (strcmp(action, "CALL") == 0) {
        return player->current_bet < game->current_bet &&
               player->points >= (game->current_bet - player->current_bet);
        //RAISE: If the player has not yet matched the current bet, they can raise if they have enough points and the raise amount is valid.
    } else if (strcmp(action, "RAISE") == 0) {
        int min_raise = game->current_bet * 2 - player->current_bet;
        return raise_amount >= min_raise && raise_amount <= player->points;
    }
    return 0;
}

/*Gonna implement these function later, just wanted to get the header file done first. 
Lowkey hella work so some help would be appreciated. Some function are set to return 0 for now
so nothing breaks (hopefully)
-jim
*/
int poker_apply_action(GameState *game, int seat, const char *action, int raise_amount){
    return 0;
}

int poker_is_betting_round_finished(const GameState *game){
    return 0;
}

void poker_advance_phase(GameState *game);

void poker_resolve_showdown(GameState *game);

int poker_find_showdown_winners(const GameState *game, int winners[], int max_winners){
    return 0;
}

int poker_count_active_players(const GameState *game){
    return 0;
}

int poker_get_first_active_player(const GameState *game){
    return 0;
}

int poker_get_next_active_player(const GameState *game, int seat){
    return 0;
}

int poker_compare_hands(const PokerHandValue *a, const PokerHandValue *b){
    return 0;
}

void poker_evaluate_hand(const Card cards[7], PokerHandValue *value);

const char *poker_hand_rank_to_string(PokerHandRank rank){
    return "Not implemented yet";
}