#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "cards.h"
#include "deck.h"

#define MAX_PLAYERS 6
#define MAX_NAME_LEN 32
#define MAX_TABLE_NAME_LEN 64
#define MAX_LOG_PATH_LEN 128
#define PRIVATE_HAND_SIZE 2
#define COMMUNITY_CARD_SIZE 5

typedef enum {
    PHASE_LOBBY = 0,
    PHASE_PREFLOP,
    PHASE_FLOP,
    PHASE_TURN,
    PHASE_RIVER,
    PHASE_SHOWDOWN,
    PHASE_GAME_OVER
} GamePhase;

typedef enum {
    PLAYER_EMPTY = 0,
    PLAYER_CONNECTED,
    PLAYER_ACTIVE,
    PLAYER_FOLDED,
    PLAYER_LEFT
} PlayerStatus;

typedef enum {
    ABILITY_NONE = 0,
    ABILITY_SNIFF,
    ABILITY_ANT_TRAIL,
    ABILITY_POSE,
    ABILITY_LONG_TONGUE,
    ABILITY_WILD_GRAB
} AbilityType;

typedef struct {
    AbilityType type;
    int used;
    int owner_seat;
    int target_seat;
    int param;
} AbilityCard;

typedef struct {
    int seat;
    int socket_fd;
    char name[MAX_NAME_LEN];
    int points;
    int current_bet;
    int total_bet;
    Card hand[PRIVATE_HAND_SIZE];
    AbilityCard ability;
    PlayerStatus status;
} Player;

typedef struct {
    int port;
    char table_name[MAX_TABLE_NAME_LEN];
    int starting_points;
    int bot_count;
    char log_path[MAX_LOG_PATH_LEN];
} ServerConfig;

typedef struct {
    int server_fd;
    ServerConfig config;
    GamePhase phase;
    Player players[MAX_PLAYERS];
    int player_count;
    int current_turn;
    int dealer_seat;
    int pot;
    int current_bet;
    int acted_this_round[MAX_PLAYERS];
    Deck deck;
    Card community_cards[COMMUNITY_CARD_SIZE];
    int community_count;
    int last_winner_seat;
    int last_winning_hand_rank;
    char last_winner_text[256];
} GameState;

void init_server_config(ServerConfig *config);
void init_game_state(GameState *game, const ServerConfig *config, int server_fd);

int find_empty_seat(const GameState *game);
int add_player(GameState *game, int socket_fd, const char *name);
void remove_player(GameState *game, int seat);

void start_new_hand(GameState *game);
void deal_private_cards(GameState *game);
void deal_flop(GameState *game);
void deal_turn(GameState *game);
void deal_river(GameState *game);

Player *get_player_by_seat(GameState *game, int seat);
const char *game_phase_to_string(GamePhase phase);
const char *ability_to_string(AbilityType ability);

void build_public_game_state(const GameState *game, char *buffer, int buffer_size);
void build_private_hand_message(const GameState *game, int seat, char *buffer, int buffer_size);

#endif
