#ifndef CLIENT_STATE_H
#define CLIENT_STATE_H

/*
 * client_state.h
 *
 * This file defines the ClientState structure.
 * ClientState stores the local information that the client knows about the game.
 *
 * Important:
 * The client does NOT control the official game state.
 * The server controls the official state.
 * The client only stores what it receives from the server.
 */

#include "cards.h"
#include "game_state.h"

/* Maximum size for a player's display name. */
#define CLIENT_NAME_LEN 32

/* Maximum size for a client status message. */
#define CLIENT_STATUS_LEN 256

/* Maximum number of players the client expects to display. */
#define CLIENT_MAX_PLAYERS 6

/*
 * ClientState stores the local client-side view of the game.
 *
 * This includes:
 * - whether the client is connected
 * - the socket file descriptor
 * - the player's assigned seat
 * - the player's name
 * - visible game information like phase, pot, turn, and player count
 * - private cards assigned to this client
 * - the client's Anteater ability card
 * - a status message for terminal/GUI display
 */
typedef struct {
    /* 1 if connected to the server, 0 otherwise. */
    int connected;

    /* Socket file descriptor used to communicate with the server. */
    int socket_fd;

    /* Seat number assigned by the server. -1 means no seat assigned yet. */
    int seat;

    /* Player display name. */
    char player_name[CLIENT_NAME_LEN];

    /* Current game phase received from the server. */
    GamePhase phase;

    /* Current pot amount received from the server. */
    int pot;

    /* Total amount in side pots received from the server. */
    int side_pot;

    /* Seat number of the current player whose turn it is. */
    int current_turn;

    /* Number of connected players. */
    int player_count;

    /* Number of community cards currently revealed. */
    int community_count;

    /* The player's community cards */
    Card community_cards[COMMUNITY_CARD_SIZE];

    /* The player's two private poker cards. */
    Card private_cards[PRIVATE_HAND_SIZE];

    /* The player's Anteater ability card type. */
    AbilityType ability;

    /* Short message describing current client status. */
    char status_message[CLIENT_STATUS_LEN];

    /* 1 if this player is ready to start, 0 otherwise (lobby only). */
    int ready;
} ClientState;

/*
 * Initializes a ClientState with safe default values.
 */
void init_client_state(ClientState *client);

/*
 * Sets the player's display name.
 */
void set_client_name(ClientState *client, const char *name);

/*
 * Sets the client status message.
 */
void set_client_status(ClientState *client, const char *message);

/*
 * Prints the current local client state.
 * This is useful for testing before the GUI is implemented.
 */
void print_client_state(const ClientState *client);

#endif