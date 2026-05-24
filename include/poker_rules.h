#ifndef POKER_RULES_H
#define POKER_RULES_H

#include "game_state.h"
#include "cards.h"


typedef enum {
    POKER_ACTION_ILLEGAL = 0,
    POKER_ACTION_FOLD,
    POKER_ACTION_CHECK,
    POKER_ACTION_CALL,
    POKER_ACTION_RAISE
} PokerActionType;

typedef enum {
    HAND_RANK_HIGH_CARD = 0,
    HAND_RANK_PAIR,
    HAND_RANK_TWO_PAIR,
    HAND_RANK_THREE_OF_A_KIND,
    HAND_RANK_STRAIGHT,
    HAND_RANK_FLUSH,
    HAND_RANK_FULL_HOUSE,
    HAND_RANK_FOUR_OF_A_KIND,
    HAND_RANK_STRAIGHT_FLUSH
} PokerHandRank;

typedef struct {
    PokerHandRank rank;
    int values[5];
} PokerHandValue;

int poker_action_is_legal(const GameState *game,int seat, const char *action, int raise_amount);

int poker_apply_action(GameState *game, int seat, const char *action, int raise_amount);

int poker_is_betting_round_finished(const GameState *game);

void poker_advance_phase(GameState *game);

void poker_resolve_showdown(GameState *game);

int poker_find_showdown_winners(const GameState *game, int winners[], int max_winners);

int poker_count_active_players(const GameState *game);

int poker_get_first_active_player(const GameState *game);

int poker_get_next_active_player(const GameState *game, int seat);

int poker_compare_hands(const PokerHandValue *a, const PokerHandValue *b);

void poker_evaluate_hand(const Card cards[7], PokerHandValue *value);

const char *poker_hand_rank_to_string(PokerHandRank rank);


#endif
