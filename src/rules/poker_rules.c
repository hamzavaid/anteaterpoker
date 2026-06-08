#include <stdio.h>
#include <string.h>

#include "poker_rules.h"
#include "game_state.h"
#include "cards.h"
#include "deck.h"

/*
 * poker_rules.c is the authoritative rules engine for a hand.
 * The server asks this file whether an action is legal, applies chip movement,
 * advances betting rounds, deals community cards, and resolves showdown.
 */

static int active_player_has_chips(const Player *player)
{
    return player->status == PLAYER_ACTIVE && player->points > 0;
}

static void reset_round_bets(GameState *game)
{
    /* Each street has its own current bet. The pot keeps the chips. */
    game->current_bet = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->players[i].current_bet = 0;
        game->acted_this_round[i] = 0;
    }
}

int poker_action_is_legal(const GameState *game, int seat, const char *action, int raise_amount)
{
    if (game == NULL || action == NULL) {
        return 0;
    }
    if (seat < 0 || seat >= MAX_PLAYERS) {
        return 0;
    }
    if (game->current_turn != seat) {
        return 0;
    }
    if (game->phase < PHASE_PREFLOP || game->phase > PHASE_RIVER) {
        return 0;
    }

    const Player *player = &game->players[seat];
    if (player->status != PLAYER_ACTIVE) {
        return 0;
    }

    if (strcmp(action, "FOLD") == 0) {
        return 1;
    }
    if (strcmp(action, "CHECK") == 0) {
        return player->current_bet >= game->current_bet;
    }
    if (strcmp(action, "CALL") == 0) {
        int to_call = game->current_bet - player->current_bet;
        /* Allow calling by going all-in (partial call) when player doesn't have full amount. */
        return to_call > 0 && player->points > 0;
    }
    if (strcmp(action, "RAISE") == 0) {
        int new_total = raise_amount;
        int cost = new_total - player->current_bet;
        int min_total = game->current_bet == 0 ? 1 : game->current_bet + 1;
        return new_total >= min_total && cost > 0 && cost <= player->points;
    }

    return 0;
}

int poker_apply_action(GameState *game, int seat, const char *action, int raise_amount)
{
    if (!poker_action_is_legal(game, seat, action, raise_amount)) {
        return 0;
    }

    Player *player = &game->players[seat];

    if (strcmp(action, "FOLD") == 0) {
        player->status = PLAYER_FOLDED;
        game->acted_this_round[seat] = 1;
    } else if (strcmp(action, "CHECK") == 0) {
        game->acted_this_round[seat] = 1;
    } else if (strcmp(action, "CALL") == 0) {
        int to_call = game->current_bet - player->current_bet;
        int actual = to_call;
        if (player->points < actual) {
            actual = player->points; /* all-in partial call */
        }
        player->points -= actual;
        player->current_bet += actual;
        player->total_bet += actual;
        game->pot += actual;
        game->acted_this_round[seat] = 1;
    } else if (strcmp(action, "RAISE") == 0) {
        int cost = raise_amount - player->current_bet;
        int actual = cost;
        if (player->points < actual) {
            actual = player->points; /* should not happen if legality enforced, but guard */
        }
        player->points -= actual;
        player->current_bet = player->current_bet + actual;
        player->total_bet += actual;
        game->pot += actual;
        game->current_bet = raise_amount;
        /* A raise reopens action for everyone else still in the hand. */
        for (int i = 0; i < MAX_PLAYERS; i++) {
            game->acted_this_round[i] = 0;
        }
        game->acted_this_round[seat] = 1;
    }

    if (poker_count_active_players(game) <= 1) {
        /* Everyone else folded, so the remaining active player wins now. */
        int winner = poker_get_first_active_player(game);
        if (winner >= 0) {
            game->players[winner].points += game->pot;
            game->pot = 0;
        }
        /* Record winner for client display when hand ends due to folds. */
        if (winner >= 0) {
            game->last_winner_seat = winner;
            game->last_winning_hand_rank = -1;
        }
        game->phase = PHASE_GAME_OVER;
        game->current_turn = -1;
        return 1;
    }

    if (poker_is_betting_round_finished(game)) {
        /* All active players have acted and matched the bet. Move streets. */
        poker_advance_phase(game);
    } else {
        game->current_turn = poker_get_next_active_player(game, seat);
    }

    return 1;
}

int poker_is_betting_round_finished(const GameState *game)
{
    if (game == NULL) {
        return 0;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player *player = &game->players[i];
        if (player->status == PLAYER_ACTIVE && player->points > 0) {
            if (!game->acted_this_round[i]) {
                return 0;
            }
            if (player->current_bet < game->current_bet) {
                return 0;
            }
        }
    }

    return poker_count_active_players(game) > 0;
}

void poker_advance_phase(GameState *game)
{
    if (game == NULL) {
        return;
    }

    reset_round_bets(game);

    if (game->phase == PHASE_PREFLOP) {
        game->community_cards[0] = deal_card(&game->deck);
        game->community_cards[1] = deal_card(&game->deck);
        game->community_cards[2] = deal_card(&game->deck);
        game->community_count = 3;
        game->phase = PHASE_FLOP;
    } else if (game->phase == PHASE_FLOP) {
        game->community_cards[3] = deal_card(&game->deck);
        game->community_count = 4;
        game->phase = PHASE_TURN;
    } else if (game->phase == PHASE_TURN) {
        game->community_cards[4] = deal_card(&game->deck);
        game->community_count = 5;
        game->phase = PHASE_RIVER;
    } else if (game->phase == PHASE_RIVER) {
        game->phase = PHASE_SHOWDOWN;
        poker_resolve_showdown(game);
        return;
    }

    game->current_turn = poker_get_first_active_player(game);
}

void poker_resolve_showdown(GameState *game)
{
    if (game == NULL) return;

    game->last_winner_text[0] = '\0';

    /* Build list of distinct contribution levels from players' total_bet. */
    int contrib[MAX_PLAYERS];
    int levels[MAX_PLAYERS];
    int nlevels = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        contrib[i] = game->players[i].total_bet;
        if (contrib[i] > 0) {
            int found = 0;
            for (int j = 0; j < nlevels; j++) {
                if (levels[j] == contrib[i]) { found = 1; break; }
            }
            if (!found) levels[nlevels++] = contrib[i];
        }
    }

    if (nlevels == 0) {
        // Suppose everybody bet 0, but we still want to show winner
        levels[0] = 0;
        nlevels = 1;
    }

    /* Sort levels ascending (simple selection sort - small arrays). */
    for (int i = 0; i < nlevels - 1; i++) {
        int min = i;
        for (int j = i + 1; j < nlevels; j++) {
            if (levels[j] < levels[min]) min = j;
        }
        if (min != i) {
            int t = levels[i]; levels[i] = levels[min]; levels[min] = t;
        }
    }

    int prev = 0;
    int last_winner = -1;
    int last_winning_rank = -1;
    int summary_len = 0;

    for (int li = 0; li < nlevels; li++) {
        int level = levels[li];
        int contributors = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (contrib[i] >= level) contributors++;
        }
        if (contributors == 0) continue;

        int pot_amount = (level - prev) * contributors;

        /* Build list of contenders eligible for this pot: active (not folded) players with contrib >= level */
        int contenders[MAX_PLAYERS];
        int ncont = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->players[i].status == PLAYER_ACTIVE && contrib[i] >= level) {
                contenders[ncont++] = i;
            }
        }

        /* If there are no active contenders for this pot (rare), include any contributor */
        if (ncont == 0) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (contrib[i] >= level) contenders[ncont++] = i;
            }
        }

        if (ncont == 0) {
            /* Still nothing - skip */
            prev = level;
            continue;
        }

        /* Determine winners among contenders */
        int winners[MAX_PLAYERS];
        int winner_count = 0;
        PokerHandValue best;
        int found = 0;

        for (int ci = 0; ci < ncont; ci++) {
            int seat = contenders[ci];
            Card cards[7];
            cards[0] = game->players[seat].hand[0];
            cards[1] = game->players[seat].hand[1];
            for (int c = 0; c < COMMUNITY_CARD_SIZE; c++) {
                cards[c + 2] = game->community_cards[c];
            }

            PokerHandValue value;
            poker_evaluate_hand(cards, &value);

            if (!found || poker_compare_hands(&value, &best) > 0) {
                best = value;
                winners[0] = seat;
                winner_count = 1;
                found = 1;
            } else if (poker_compare_hands(&value, &best) == 0 && winner_count < MAX_PLAYERS) {
                winners[winner_count++] = seat;
            }
        }

        if (winner_count > 0) {
            int share = pot_amount / winner_count;
            int remainder = pot_amount % winner_count;
            for (int wi = 0; wi < winner_count; wi++) {
                game->players[winners[wi]].points += share;
                if (wi == 0) game->players[winners[wi]].points += remainder;
            }

            if (summary_len < (int)sizeof(game->last_winner_text) - 1) {
                int wrote = 0;
                if (winner_count == 1) {
                    wrote = snprintf(
                        game->last_winner_text + summary_len,
                        sizeof(game->last_winner_text) - (size_t)summary_len,
                        "%sSeat %d wins %d",
                        summary_len > 0 ? "; " : "",
                        winners[0] + 1,
                        pot_amount
                    );
                } else {
                    wrote = snprintf(
                        game->last_winner_text + summary_len,
                        sizeof(game->last_winner_text) - (size_t)summary_len,
                        "%sSeats",
                        summary_len > 0 ? "; " : ""
                    );
                    if (wrote > 0) {
                        summary_len += wrote;
                        for (int wi = 0; wi < winner_count && summary_len < (int)sizeof(game->last_winner_text) - 1; wi++) {
                            wrote = snprintf(
                                game->last_winner_text + summary_len,
                                sizeof(game->last_winner_text) - (size_t)summary_len,
                                "%s%d",
                                (wi == 0) ? " " : ",",
                                winners[wi] + 1
                            );
                            if (wrote > 0) {
                                summary_len += wrote;
                            }
                        }
                        if (summary_len < (int)sizeof(game->last_winner_text) - 1) {
                            wrote = snprintf(
                                game->last_winner_text + summary_len,
                                sizeof(game->last_winner_text) - (size_t)summary_len,
                                " split %d",
                                pot_amount
                            );
                        }
                    }
                }
                if (wrote > 0) {
                    summary_len += wrote;
                    if (summary_len >= (int)sizeof(game->last_winner_text)) {
                        summary_len = (int)sizeof(game->last_winner_text) - 1;
                        game->last_winner_text[summary_len] = '\0';
                    }
                }
            }

            /* record last (highest-level) winner info for client display */
            last_winner = winners[0];
            last_winning_rank = best.rank;
        }

        prev = level;
    }

    /* Clear pot and bookkeeping */
    game->pot = 0;
    game->current_turn = -1;
    game->phase = PHASE_GAME_OVER;
    if (last_winner >= 0) {
        game->last_winner_seat = last_winner;
        game->last_winning_hand_rank = last_winning_rank;
    }
    if (game->last_winner_text[0] == '\0' && last_winner >= 0) {
        snprintf(game->last_winner_text, sizeof(game->last_winner_text), "Seat %d wins the hand", last_winner + 1);
    }
}

int poker_find_showdown_winners(const GameState *game, int winners[], int max_winners)
{
    if (game == NULL || winners == NULL || max_winners <= 0 || game->community_count < 5) {
        return 0;
    }

    PokerHandValue best;
    int found = 0;
    int winner_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player *player = &game->players[i];
        if (player->status != PLAYER_ACTIVE) {
            continue;
        }

        Card cards[7];
        cards[0] = player->hand[0];
        cards[1] = player->hand[1];
        for (int c = 0; c < COMMUNITY_CARD_SIZE; c++) {
            cards[c + 2] = game->community_cards[c];
        }

        PokerHandValue value;
        poker_evaluate_hand(cards, &value);

        if (!found || poker_compare_hands(&value, &best) > 0) {
            best = value;
            winners[0] = i;
            winner_count = 1;
            found = 1;
        } else if (poker_compare_hands(&value, &best) == 0 && winner_count < max_winners) {
            winners[winner_count++] = i;
        }
    }

    return winner_count;
}

int poker_count_active_players(const GameState *game)
{
    int count = 0;
    if (game == NULL) {
        return 0;
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status == PLAYER_ACTIVE) {
            count++;
        }
    }
    return count;
}

int poker_get_first_active_player(const GameState *game)
{
    if (game == NULL) {
        return -1;
    }
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (active_player_has_chips(&game->players[i])) {
            return i;
        }
    }
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (game->players[i].status == PLAYER_ACTIVE) {
            return i;
        }
    }
    return -1;
}

int poker_get_next_active_player(const GameState *game, int seat)
{
    if (game == NULL) {
        return -1;
    }
    int start = (seat + 1 + MAX_PLAYERS) % MAX_PLAYERS;
    int i = start;
    do {
        if (active_player_has_chips(&game->players[i])) {
            return i;
        }
        i = (i + 1) % MAX_PLAYERS;
    } while (i != start);
    return poker_get_first_active_player(game);
}

int poker_compare_hands(const PokerHandValue *a, const PokerHandValue *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    if (a->rank > b->rank) {
        return 1;
    }
    if (a->rank < b->rank) {
        return -1;
    }
    for (int i = 0; i < 5; ++i) {
        if (a->values[i] > b->values[i]) {
            return 1;
        }
        if (a->values[i] < b->values[i]) {
            return -1;
        }
    }
    return 0;
}

static void poker_evaluate_fixed_hand(const Card cards[7], PokerHandValue *value)
{
    if (!value) return;
    int rank_count[15] = {0};
    int suit_count[4] = {0};
    for (int i = 0; i < 7; ++i) {
        if (cards[i].rank >= RANK_TWO && cards[i].rank <= RANK_ACE)
            rank_count[cards[i].rank]++;
        if (cards[i].suit >= SUIT_HEARTS && cards[i].suit <= SUIT_SPADES)
            suit_count[cards[i].suit]++;
    }

    int flush_suit = -1;
    for (int s = 0; s < 4; ++s) {
        if (suit_count[s] >= 5) {
            flush_suit = s;
            break;
        }
    }

    int straight_high = 0, straight_flush_high = 0;
    for (int start = 14; start >= 5; --start) {
        int found = 1;
        for (int j = 0; j < 5; ++j) {
            if (rank_count[start - j] == 0) {
                found = 0;
                break;
            }
        }
        if (found) {
            straight_high = start;
            if (flush_suit != -1) {
                int count = 0;
                for (int i = 0; i < 7; ++i) {
                    if ((int)cards[i].suit == flush_suit && (int)cards[i].rank <= start && (int)cards[i].rank >= start - 4)
                        count++;
                }
                if (count >= 5) straight_flush_high = start;
            }
            break;
        }
    }
    if (!straight_high && rank_count[RANK_ACE] && rank_count[RANK_TWO] && rank_count[RANK_THREE] && rank_count[RANK_FOUR] && rank_count[RANK_FIVE]) {
        straight_high = 5;
        if (flush_suit != -1) {
            int count = 0;
            for (int i = 0; i < 7; ++i) {
                if ((int)cards[i].suit == flush_suit &&
                    (cards[i].rank == RANK_ACE || (cards[i].rank >= RANK_TWO && cards[i].rank <= RANK_FIVE)))
                    count++;
            }
            if (count >= 5) straight_flush_high = 5;
        }
    }

    int pairs[3] = {0}, npairs = 0, trips[2] = {0}, ntrips = 0, quad = 0;
    for (int r = 14; r >= 2; --r) {
        if (rank_count[r] == 4) quad = r;
        else if (rank_count[r] == 3 && ntrips < 2) trips[ntrips++] = r;
        else if (rank_count[r] == 2 && npairs < 3) pairs[npairs++] = r;
    }

    if (straight_flush_high) {
        value->rank = HAND_RANK_STRAIGHT_FLUSH;
        value->values[0] = straight_flush_high;
        for (int i = 1; i < 5; ++i) value->values[i] = 0;
        return;
    }
    if (quad) {
        value->rank = HAND_RANK_FOUR_OF_A_KIND;
        value->values[0] = quad;
        for (int r = 14; r >= 2; --r) {
            if (r != quad && rank_count[r]) { value->values[1] = r; break; }
        }
        for (int i = 2; i < 5; ++i) value->values[i] = 0;
        return;
    }
    if (ntrips && npairs) {
        value->rank = HAND_RANK_FULL_HOUSE;
        value->values[0] = trips[0];
        value->values[1] = pairs[0];
        for (int i = 2; i < 5; ++i) value->values[i] = 0;
        return;
    }
    if (ntrips > 1) {
        value->rank = HAND_RANK_FULL_HOUSE;
        value->values[0] = trips[0];
        value->values[1] = trips[1];
        for (int i = 2; i < 5; ++i) value->values[i] = 0;
        return;
    }
    if (flush_suit != -1) {
        value->rank = HAND_RANK_FLUSH;
        int count = 0;
        for (int r = 14; r >= 2 && count < 5; --r) {
            for (int i = 0; i < 7 && count < 5; ++i) {
                if ((int)cards[i].suit == flush_suit && (int)cards[i].rank == r) {
                    value->values[count++] = r;
                }
            }
        }
        while (count < 5) value->values[count++] = 0;
        return;
    }
    if (straight_high) {
        value->rank = HAND_RANK_STRAIGHT;
        value->values[0] = straight_high;
        for (int i = 1; i < 5; ++i) value->values[i] = 0;
        return;
    }
    if (ntrips) {
        value->rank = HAND_RANK_THREE_OF_A_KIND;
        value->values[0] = trips[0];
        int count = 1;
        for (int r = 14; r >= 2 && count < 5; --r) {
            if (r != trips[0] && rank_count[r]) value->values[count++] = r;
        }
        while (count < 5) value->values[count++] = 0;
        return;
    }
    if (npairs >= 2) {
        value->rank = HAND_RANK_TWO_PAIR;
        value->values[0] = pairs[0];
        value->values[1] = pairs[1];
        int count = 2;
        for (int r = 14; r >= 2 && count < 5; --r) {
            if (r != pairs[0] && r != pairs[1] && rank_count[r]) value->values[count++] = r;
        }
        while (count < 5) value->values[count++] = 0;
        return;
    }
    if (npairs == 1) {
        value->rank = HAND_RANK_PAIR;
        value->values[0] = pairs[0];
        int count = 1;
        for (int r = 14; r >= 2 && count < 5; --r) {
            if (r != pairs[0] && rank_count[r]) value->values[count++] = r;
        }
        while (count < 5) value->values[count++] = 0;
        return;
    }
    value->rank = HAND_RANK_HIGH_CARD;
    int count = 0;
    for (int r = 14; r >= 2 && count < 5; --r) {
        if (rank_count[r]) value->values[count++] = r;
    }
    while (count < 5) value->values[count++] = 0;
}

void poker_evaluate_hand(const Card cards[7], PokerHandValue *value)
{
    int wildcard_index = -1;

    if (value == NULL) {
        return;
    }

    /*
     * Wild Grab stores a wildcard as an invalid card in the player's private
     * hand. For evaluation, try every possible real card and keep the best.
     */
    for (int i = 0; i < 7; i++) {
        if (!is_valid_card(cards[i])) {
            wildcard_index = i;
            break;
        }
    }

    if (wildcard_index < 0) {
        poker_evaluate_fixed_hand(cards, value);
        return;
    }

    int found = 0;
    PokerHandValue best;
    Card trial[7];
    for (int i = 0; i < 7; i++) {
        trial[i] = cards[i];
    }

    for (Suit suit = SUIT_HEARTS; suit <= SUIT_SPADES; suit++) {
        for (Rank rank = RANK_TWO; rank <= RANK_ACE; rank++) {
            PokerHandValue candidate;
            trial[wildcard_index] = create_card(rank, suit);
            poker_evaluate_fixed_hand(trial, &candidate);
            if (!found || poker_compare_hands(&candidate, &best) > 0) {
                best = candidate;
                found = 1;
            }
        }
    }

    *value = best;
}

const char *poker_hand_rank_to_string(PokerHandRank rank)
{
    switch (rank) {
        case HAND_RANK_HIGH_CARD: return "High Card";
        case HAND_RANK_PAIR: return "Pair";
        case HAND_RANK_TWO_PAIR: return "Two Pair";
        case HAND_RANK_THREE_OF_A_KIND: return "Three of a Kind";
        case HAND_RANK_STRAIGHT: return "Straight";
        case HAND_RANK_FLUSH: return "Flush";
        case HAND_RANK_FULL_HOUSE: return "Full House";
        case HAND_RANK_FOUR_OF_A_KIND: return "Four of a Kind";
        case HAND_RANK_STRAIGHT_FLUSH: return "Straight Flush";
        default: return "Unknown";
    }
}
