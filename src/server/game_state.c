/*
 * game_state.c
 *
 * Server-side game-state management for Anteater Poker.
 *
 * This file owns the data and helper functions that describe the current
 * poker table: connected players, current phase, deck, pot, current turn,
 * community cards, private hands, and Anteater ability cards.
 *
 * The networking code should call these functions instead of directly
 * changing every field itself. This keeps the official game state centralized
 * on the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_state.h"
#include "poker_rules.h"

/*
 * init_server_config
 *
 * Fills a ServerConfig structure with default startup values.
 * These defaults can later be overwritten by command-line arguments
 * in server.c.
 *
 * Default port: 10010
 * Default table name: ZotHouse
 * Default starting points: 1000
 */
void init_server_config(ServerConfig *config)
{
    /* Always check pointers before using them to avoid crashes. */
    if (config == NULL) {
        return;
    }

    /* Default port range for the project is 10010-10019.
     * We use 10010 as the default port.
     */
    config->port = 10010;

    /* Store the default table name safely inside the fixed-size buffer. */
    snprintf(config->table_name, MAX_TABLE_NAME_LEN, "ZotHouse");

    /* Every player starts with this many points unless changed by --points. */
    config->starting_points = 1000;

    /* Bot support is planned, but default starts with no bots. */
    config->bot_count = 0;

    /* Default log file path for debugging output. */
    snprintf(config->log_path, MAX_LOG_PATH_LEN, "logs/game.log");
}

/*
 * init_game_state
 *
 * Initializes the main GameState structure for the server.
 * This sets up the table before any clients join.
 *
 * Parameters:
 *   game      - pointer to the GameState to initialize
 *   config    - server configuration values
 *   server_fd - listening socket file descriptor
 */
void init_game_state(GameState *game, const ServerConfig *config, int server_fd)
{
    if (game == NULL || config == NULL) {
        return;
    }

    /* Store the listening socket and copy the server configuration. */
    game->server_fd = server_fd;
    game->config = *config;

    /* New server starts in the lobby with no active hand yet. */
    game->phase = PHASE_LOBBY;
    game->player_count = 0;

    /* -1 means no current player turn is assigned yet. */
    game->current_turn = -1;

    /* Dealer seat starts at 0 in the alpha build. */
    game->dealer_seat = 0;

    /* No bets or community cards exist at startup. */
    game->pot = 0;
    game->current_bet = 0;
    game->community_count = 0;
    game->last_winner_seat = -1;
    game->last_winning_hand_rank = -1;
    game->last_winner_text[0] = '\0';
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->acted_this_round[i] = 0;
    }

    /* Initialize the standard 52-card deck. */
    init_deck(&game->deck);

    /* Clear every possible player seat. */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->players[i].seat = i;
        game->players[i].socket_fd = -1;
        game->players[i].name[0] = '\0';
        game->players[i].points = config->starting_points;
        game->players[i].current_bet = 0;
        game->players[i].total_bet = 0;
        game->players[i].status = PLAYER_EMPTY;

        /* Reset each player's Anteater ability card. */
        game->players[i].ability.type = ABILITY_NONE;
        game->players[i].ability.used = 0;
        game->players[i].ability.owner_seat = i;
        game->players[i].ability.target_seat = -1;
        game->players[i].ability.param = 0;
    }

    /* Mark all community-card slots as invalid until cards are dealt. */
    for (int i = 0; i < COMMUNITY_CARD_SIZE; i++) {
        game->community_cards[i] = create_card(RANK_INVALID, SUIT_INVALID);
    }
}

/*
 * find_empty_seat
 *
 * Searches the table for the first available seat.
 *
 * Returns:
 *   index of the open seat, or -1 if the table is full or game is NULL.
 */
int find_empty_seat(const GameState *game)
{
    if (game == NULL) {
        return -1;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status == PLAYER_EMPTY) {
            return i;
        }
    }

    return -1;
}

/*
 * add_player
 *
 * Adds a new connected client as a player at the first empty seat.
 *
 * Parameters:
 *   game      - official game state
 *   socket_fd - client socket descriptor
 *   name      - player display name
 *
 * Returns:
 *   assigned seat number, or -1 if no seat is available.
 */
int add_player_at(GameState *game, int socket_fd, const char *name, int requested_seat)
{
    if (game == NULL || name == NULL) {
        return -1;
    }

    if (requested_seat < 0) {
        requested_seat = find_empty_seat(game);
    }

    if (requested_seat < 0 || requested_seat >= MAX_PLAYERS) {
        return -1;
    }

    Player *player = &game->players[requested_seat];
    if (player->status != PLAYER_EMPTY) {
        return -1;
    }

    /* Fill in basic player information at the requested seat. */
    player->seat = requested_seat;
    player->socket_fd = socket_fd;
    snprintf(player->name, MAX_NAME_LEN, "%s", name);
    player->points = game->config.starting_points;
    player->current_bet = 0;
    player->total_bet = 0;
    player->player_ready = 0;
    player->status = PLAYER_CONNECTED;

    /* Player has no ability until a hand starts. */
    player->ability.type = ABILITY_NONE;
    player->ability.used = 0;
    player->ability.owner_seat = requested_seat;
    player->ability.target_seat = -1;
    player->ability.param = 0;

    game->player_count++;

    return requested_seat;
}

int add_player(GameState *game, int socket_fd, const char *name)
{
    if (game == NULL || name == NULL) {
        return -1;
    }

    /* Find where this player can sit. */
    int seat = find_empty_seat(game);

    if (seat < 0) {
        return -1;
    }

    return add_player_at(game, socket_fd, name, seat);
}

/*
 * remove_player
 *
 * Clears a player seat when a client leaves or disconnects.
 *
 * Parameters:
 *   game - official game state
 *   seat - seat number to clear
 */
void remove_player(GameState *game, int seat)
{
    if (game == NULL || seat < 0 || seat >= MAX_PLAYERS) {
        return;
    }

    Player *player = &game->players[seat];

    /* Only remove seats that are actually occupied. */
    if (player->status != PLAYER_EMPTY) {
        player->socket_fd = -1;
        player->name[0] = '\0';
        player->points = game->config.starting_points;
        player->current_bet = 0;
        player->total_bet = 0;
        player->status = PLAYER_EMPTY;
        player->ability.type = ABILITY_NONE;

        if (game->player_count > 0) {
            game->player_count--;
        }

        /* If the table is empty, reset the whole table back to lobby state. */
        if (game->player_count == 0) {
            game->phase = PHASE_LOBBY;
            game->current_turn = -1;
            game->pot = 0;
            game->current_bet = 0;
            game->community_count = 0;
            game->last_winner_seat = -1;
            game->last_winning_hand_rank = -1;
            game->last_winner_text[0] = '\0';

            for (int i = 0; i < MAX_PLAYERS; i++) {
                game->acted_this_round[i] = 0;
                game->players[i].current_bet = 0;
                game->players[i].total_bet = 0;
                game->players[i].ability.type = ABILITY_NONE;
                game->players[i].ability.used = 0;
                game->players[i].ability.target_seat = -1;
                game->players[i].ability.param = 0;

                for (int j = 0; j < PRIVATE_HAND_SIZE; j++) {
                    game->players[i].hand[j] = create_card(RANK_INVALID, SUIT_INVALID);
                }
            }

            for (int i = 0; i < COMMUNITY_CARD_SIZE; i++) {
                game->community_cards[i] = create_card(RANK_INVALID, SUIT_INVALID);
            }
        }
    }
}

/*
 * start_new_hand
 *
 * Starts a new poker hand:
 *   1. Creates and shuffles a fresh deck.
 *   2. Resets pot, bets, and community cards.
 *   3. Marks connected players as active.
 *   4. Deals two private cards and one ability card.
 *   5. Sets the first active player as the current turn.
 */
void start_new_hand(GameState *game)
{
    if (game == NULL) {
        return;
    }

    /* Fresh deck for every new hand. */
    init_deck(&game->deck);
    shuffle_deck(&game->deck);

    /* New hand starts before the flop. */
    game->phase = PHASE_PREFLOP;
    game->pot = 0;
    game->current_bet = 0;
    game->community_count = 0;
    game->last_winner_seat = -1;
    game->last_winning_hand_rank = -1;
    game->last_winner_text[0] = '\0';
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->acted_this_round[i] = 0;
    }

    /* Clear old community cards. */
    for (int i = 0; i < COMMUNITY_CARD_SIZE; i++) {
        game->community_cards[i] = create_card(RANK_INVALID, SUIT_INVALID);
    }

    /* Reset player state for the new hand. Any non-empty seat becomes ACTIVE. */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status != PLAYER_EMPTY) {

            // Eliminate players if they have no chips
            if (game->players[i].points <= 0) {
                printf("[SERVER] %s eliminated!\n", game->players[i].name);
                remove_player(game, i);
                continue; 
            }

            game->players[i].status = PLAYER_ACTIVE;
            game->players[i].current_bet = 0;
            game->players[i].total_bet = 0;
            game->players[i].ability.used = 0;
            game->players[i].ability.owner_seat = i;
            game->players[i].ability.target_seat = -1;
        }
    }

    /* Deal two normal private cards and assign ability cards. */
    deal_private_cards(game);

    /* Assign the first active player as current turn for the alpha build. */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status == PLAYER_ACTIVE) {
            game->current_turn = i;
            break;
        }
    }
}

/*
 * deal_private_cards
 *
 * Deals two normal poker cards to every active player.
 * Also assigns one temporary Anteater ability card.
 */
void deal_private_cards(GameState *game)
{
    if (game == NULL) {
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status == PLAYER_ACTIVE) {
            /* Deal two Texas Hold'em private cards. */
            game->players[i].hand[0] = deal_card(&game->deck);
            game->players[i].hand[1] = deal_card(&game->deck);

            game->players[i].ability.type = (AbilityType)((rand() % 5) + 1);
            game->players[i].ability.used = 0;
            game->players[i].ability.owner_seat = i;
        }
    }
}

/*
 * deal_flop
 *
 * Deals the first three community cards.
 * This should only happen when no community cards have been dealt yet.
 */
void deal_flop(GameState *game)
{
    if (game == NULL) {
        return;
    }

    if (game->community_count == 0) {
        game->community_cards[0] = deal_card(&game->deck);
        game->community_cards[1] = deal_card(&game->deck);
        game->community_cards[2] = deal_card(&game->deck);
        game->community_count = 3;
        game->phase = PHASE_FLOP;
    }
}

/*
 * deal_turn
 *
 * Deals the fourth community card.
 * This should only happen after the flop exists.
 */
void deal_turn(GameState *game)
{
    if (game == NULL) {
        return;
    }

    if (game->community_count == 3) {
        game->community_cards[3] = deal_card(&game->deck);
        game->community_count = 4;
        game->phase = PHASE_TURN;
    }
}

/*
 * deal_river
 *
 * Deals the fifth and final community card.
 * This should only happen after the turn exists.
 */
void deal_river(GameState *game)
{
    if (game == NULL) {
        return;
    }

    if (game->community_count == 4) {
        game->community_cards[4] = deal_card(&game->deck);
        game->community_count = 5;
        game->phase = PHASE_RIVER;
    }
}

/*
 * get_player_by_seat
 *
 * Returns a pointer to the player in the requested seat.
 *
 * Returns:
 *   Player pointer if the seat is valid and occupied.
 *   NULL if the seat is invalid or empty.
 */
Player *get_player_by_seat(GameState *game, int seat)
{
    if (game == NULL || seat < 0 || seat >= MAX_PLAYERS) {
        return NULL;
    }

    if (game->players[seat].status == PLAYER_EMPTY) {
        return NULL;
    }

    return &game->players[seat];
}

/*
 * game_phase_to_string
 *
 * Converts the GamePhase enum into a readable string.
 * Useful for debugging and network state messages.
 */
const char *game_phase_to_string(GamePhase phase)
{
    switch (phase) {
        case PHASE_LOBBY:
            return "LOBBY";
        case PHASE_PREFLOP:
            return "PREFLOP";
        case PHASE_FLOP:
            return "FLOP";
        case PHASE_TURN:
            return "TURN";
        case PHASE_RIVER:
            return "RIVER";
        case PHASE_SHOWDOWN:
            return "SHOWDOWN";
        case PHASE_GAME_OVER:
            return "GAME_OVER";
        default:
            return "UNKNOWN";
    }
}

/*
 * ability_to_string
 *
 * Converts an AbilityType enum into a readable string.
 * Used when sending a private hand message to a client.
 */
const char *ability_to_string(AbilityType ability)
{
    switch (ability) {
        case ABILITY_NONE:
            return "NONE";
        case ABILITY_SNIFF:
            return "SNIFF";
        case ABILITY_ANT_TRAIL:
            return "ANT_TRAIL";
        case ABILITY_POSE:
            return "POSE";
        case ABILITY_LONG_TONGUE:
            return "LONG_TONGUE";
        case ABILITY_WILD_GRAB:
            return "WILD_GRAB";
        default:
            return "UNKNOWN";
    }
}

/*
 * build_public_game_state
 *
 * Builds a text message describing the public state of the game.
 * This message can be broadcast to all clients because it does not reveal
 * private cards.
 *
 * Format:
 *   STAT:-1:phase=<phase>;players=<count>;pot=<pot>;turn=<seat>;community=<count>;community_cards=<card1>,<card2>,...
 */

static int calculate_side_pot_total(const GameState *game)
{
    if (game == NULL) {
        return 0;
    }

    int is_all_in = 0;
    int min_active_bet = -1;
    int total = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->players[i].status == PLAYER_ACTIVE) {
            int bet = game->players[i].total_bet;
            
            // If an active player is out of points but has bet, they are all-in
            if (game->players[i].points == 0 && bet > 0) {
                is_all_in = 1;
            }

            // Track the smallest bet made by someone still in the hand
            if (min_active_bet < 0 || bet < min_active_bet) {
                min_active_bet = bet;
            }
        }
    }

    // Make sure somebody is still all-in, otherwise stop calculating
    if (!is_all_in || min_active_bet <= 0) {
        return 0;
    }

    int main_pot = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int bet = game->players[i].total_bet;
        if (bet > 0) {
            total += bet;
            
            // A player can only win from others exactly what they put in themselves.
            // Any excess bet from another player goes to the side pot.
            main_pot += (bet < min_active_bet) ? bet : min_active_bet;
        }
    }

    return total - main_pot;
}

void build_public_game_state(const GameState *game, char *buffer, int buffer_size)
{
    if (game == NULL || buffer == NULL || buffer_size <= 0) {
        return;
    }

    char community_cards[256] = "";
    char player_summary[512] = "";
    char showdown_cards[512] = "";
    const char *winner_hand = "";
    int visible_players = 0;
    int side_pot = calculate_side_pot_total(game);
    for (int i = 0; i < game->community_count; i++) {
        char card_str[64];
        card_to_string(game->community_cards[i], card_str, sizeof(card_str));

        if (i > 0) {
            strncat(community_cards, ",", sizeof(community_cards) - strlen(community_cards) - 1);
        }
        strncat(community_cards, card_str, sizeof(community_cards) - strlen(community_cards) - 1);
    }

    /*
     * Public player summaries let every client show opponent names, points,
     * bets, and folded/active status without exposing private cards.
     * Entry format: seat|name|points|bet|status
     */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Player *player = &game->players[i];
        char entry[96];

        if (player->status == PLAYER_EMPTY ||
            (player->status == PLAYER_CONNECTED && strcmp(player->name, "Guest") == 0)) {
            continue;
        }

        visible_players++;

        snprintf(entry, sizeof(entry), "%s%d|%s|%d|%d|%d",
                 player_summary[0] ? "," : "",
                 i,
                 player->name,
                 player->points,
                 player->current_bet,
                 player->status);
        strncat(player_summary, entry,
                sizeof(player_summary) - strlen(player_summary) - 1);
    }

    if (game->phase == PHASE_SHOWDOWN || game->phase == PHASE_GAME_OVER)
    {
        /* If a full community is present, the recorded winning hand rank is meaningful. */
        if (game->community_count == COMMUNITY_CARD_SIZE &&
            game->last_winning_hand_rank >= HAND_RANK_HIGH_CARD &&
            game->last_winning_hand_rank <= HAND_RANK_STRAIGHT_FLUSH)
        {
            winner_hand = poker_hand_rank_to_string((PokerHandRank)game->last_winning_hand_rank);
        }

        /* Include private cards for any non-empty seats so clients can reveal them at hand end.
         * This applies even when the hand ended early due to folds (community_count < 5). */
        for (int i = 0; i < MAX_PLAYERS; i++) {
            const Player *player = &game->players[i];
            char card1[64];
            char card2[64];
            char entry[160];

            if (player->status == PLAYER_EMPTY ||
                (player->status == PLAYER_CONNECTED && strcmp(player->name, "Guest") == 0)) {
                continue;
            }

            /* If both private cards are invalid, nothing to reveal. */
            if (!is_valid_card(player->hand[0]) && !is_valid_card(player->hand[1])) {
                continue;
            }

            if (!is_valid_card(player->hand[0])) {
                snprintf(card1, sizeof(card1), "wildcard");
            } else {
                card_to_string(player->hand[0], card1, sizeof(card1));
            }

            if (!is_valid_card(player->hand[1])) {
                snprintf(card2, sizeof(card2), "wildcard");
            } else {
                card_to_string(player->hand[1], card2, sizeof(card2));
            }

            snprintf(entry, sizeof(entry), "%s%d|%s|%s",
                     showdown_cards[0] ? "," : "",
                     i,
                     card1,
                     card2);
            strncat(showdown_cards, entry,
                    sizeof(showdown_cards) - strlen(showdown_cards) - 1);
        }
    }

    snprintf(
        buffer,
        buffer_size,
        "STAT:-1:phase=%s;players=%d;pot=%d;sidepot=%d;turn=%d;winner=%d;community=%d;winner_text=%s;community_cards=%s;winner_hand=%s;showdown_cards=%s;player_state=%s\n",
        game_phase_to_string(game->phase),
        visible_players,
        game->pot,
        side_pot,
        game->current_turn,
        game->last_winner_seat,
        game->community_count,
        game->last_winner_text,
        community_cards,
        winner_hand,
        showdown_cards,
        player_summary
    );
}

/*
 * build_private_hand_message
 *
 * Builds a private message for one player containing that player's
 * two private cards and Anteater ability card.
 *
 * This must only be sent to the matching client.
 *
 * Format:
 *   HAND:<seat>:<card1>,<card2>,ability=<ability>;ability_used=<0|1>;points=<points>
 */
void build_private_hand_message(const GameState *game, int seat, char *buffer, int buffer_size)
{
    if (game == NULL || buffer == NULL || buffer_size <= 0 ||
        seat < 0 || seat >= MAX_PLAYERS) {
        return;
    }

    const Player *player = &game->players[seat];

    char card1[64];
    char card2[64];

    /* Convert card structs into readable strings. Wild Grab uses invalid_card as wildcard. */
    if (!is_valid_card(player->hand[0])) {
        snprintf(card1, sizeof(card1), "wildcard");
    } else {
        card_to_string(player->hand[0], card1, sizeof(card1));
    }
    if (!is_valid_card(player->hand[1])) {
        snprintf(card2, sizeof(card2), "wildcard");
    } else {
        card_to_string(player->hand[1], card2, sizeof(card2));
    }

    snprintf(
        buffer,
        buffer_size,
        "HAND:%d:%s,%s,ability=%s;ability_used=%d;points=%d\n",
        seat,
        card1,
        card2,
        ability_to_string(player->ability.type),
        player->ability.used,
        player->points
    );
}
