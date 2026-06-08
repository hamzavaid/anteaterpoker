/*
 * client_state.c
 *
 * This file implements helper functions for managing the local client state.
 * The client state is only a local copy of information approved by the server.
 */

#include <stdio.h>
#include <string.h>

#include "client_state.h"
#include "poker_gui.h"

/*
 * Initializes a ClientState structure with default values.
 *
 * This prevents random/uninitialized memory from being used later.
 */
void init_client_state(ClientState *client)
{
    /* Make sure the pointer is valid before using it. */
    if (client == NULL) {
        return;
    }

    /* Client starts disconnected. */
    client->connected = 0;

    /* No socket is assigned yet. */
    client->socket_fd = -1;

    /* No seat is assigned yet. */
    client->seat = -1;

    /* Default player name before login. */
    snprintf(client->player_name, CLIENT_NAME_LEN, "Player");

    /* The client starts in the lobby phase. */
    client->phase = PHASE_LOBBY;

    /* No pot exists before the game starts. */
    client->pot = 0;

    /* No side pot exists before the game starts. */
    client->side_pot = 0;

    /* No current turn exists before the game starts. */
    client->current_turn = -1;

    /* No players are known yet. */
    client->player_count = 0;

    /* No community cards are shown at the start. */
    client->community_count = 0;

    /* Set private cards to invalid cards until the server sends real cards. */
    client->private_cards[0] = create_card(RANK_INVALID, SUIT_INVALID);
    client->private_cards[1] = create_card(RANK_INVALID, SUIT_INVALID);

    /* No Anteater ability is assigned yet. */
    client->ability = ABILITY_NONE;

    /* Default status message. */
    snprintf(client->status_message, CLIENT_STATUS_LEN, "Not connected.");

    /* Client starts not ready. */
    client->ready = 0;
}

/*
 * Updates the player's display name.
 */
void set_client_name(ClientState *client, const char *name)
{
    /* Make sure both pointers are valid. */
    if (client == NULL || name == NULL) {
        return;
    }

    /* Safely copy the name into the client state. */
    snprintf(client->player_name, CLIENT_NAME_LEN, "%s", name);
}

/*
 * Updates the local status message.
 */
void set_client_status(ClientState *client, const char *message)
{
    /* Make sure both pointers are valid. */
    if (client == NULL || message == NULL) {
        return;
    }

    /* Safely copy the status message into the client state. */
    snprintf(client->status_message, CLIENT_STATUS_LEN, "%s", message);
}

/*
 * Prints the current client state.
 *
 * This is mainly for debugging and terminal testing.
 * Later, the GUI can show this information visually.
 */
void print_client_state(const ClientState *client)
{
    /* Make sure the pointer is valid. */
    if (client == NULL) {
        return;
    }

    printf("\n===== Client State =====\n");

    /* Print whether the client is connected. */
    printf("Connected: %s\n", client->connected ? "Yes" : "No");

    /* Print player name and assigned seat. */
    printf("Name: %s\n", client->player_name);
    printf("Seat: %d\n", client->seat);

    /* Print public game state information. */
    printf("Phase: %s\n", game_phase_to_string(client->phase));
    printf("Pot: %d\n", client->pot);
    printf("Side Pot: %d\n", client->side_pot);
    printf("Current Turn: %d\n", client->current_turn);
    printf("Players: %d\n", client->player_count);
    printf("Community Cards: %d\n", client->community_count);

    /* Print Anteater ability information. */
    printf("Ability: %s\n", ability_to_string(client->ability));

    /* Print current client status. */
    printf("Status: %s\n", client->status_message);

    printf("========================\n\n");
}