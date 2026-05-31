#define _POSIX_C_SOURCE 200809L

/*
 * poker.c
 *
 * Terminal-based Anteater Poker client.
 *
 * This file lets a player:
 * - connect to the server
 * - send LOGIN
 * - send actions such as START, CHECK, CALL, FOLD, RAISE, and QUIT
 * - receive and process server messages
 *
 * This version fixes:
 * - STAT messages now update ClientState
 * - HAND messages now update ability/card status
 * - multiple server messages received at once are processed line by line
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <gtk/gtk.h>

#include "client_state.h"
#include "socket_client.h"
#include "poker_gui.h"

/* Default server host if the user does not provide --host. */
#define DEFAULT_HOST "localhost"

/* Default project port. */
#define DEFAULT_PORT 10010

static ClientState g_client;

static void build_card_asset_path(char *out, size_t out_size, const char *card_text)
{
    char filename[128];
    size_t j = 0;

    if (out == NULL || out_size == 0)
    {
        return;
    }

    if (card_text == NULL)
    {
        snprintf(out, out_size, "%s", "src/assets/back_of_card.png");
        return;
    }

    if (strcmp(card_text, "wildcard") == 0 || strcmp(card_text, "invalid_card") == 0)
    {
        snprintf(out, out_size, "%s", "src/assets/back_of_card.png");
        return;
    }

    for (size_t i = 0; card_text[i] != '\0' && j < sizeof(filename) - 1; i++)
    {
        unsigned char ch = (unsigned char)card_text[i];

        if (ch == ' ')
        {
            filename[j++] = '_';
        }
        else
        {
            filename[j++] = (char)tolower(ch);
        }
    }

    filename[j] = '\0';

    snprintf(out, out_size, "src/assets/%s.png", filename);
}

/*
 * Converts a phase string from the server into a GamePhase enum.
 *
 * Example:
 * "PREFLOP" becomes PHASE_PREFLOP.
 */
static GamePhase phase_from_string(const char *phase)
{
    /* If phase is NULL, use LOBBY as a safe default. */
    if (phase == NULL)
    {
        return PHASE_LOBBY;
    }

    /* Compare the server text against each known phase. */
    if (strcmp(phase, "LOBBY") == 0)
    {
        return PHASE_LOBBY;
    }

    if (strcmp(phase, "PREFLOP") == 0)
    {
        return PHASE_PREFLOP;
    }

    if (strcmp(phase, "FLOP") == 0)
    {
        return PHASE_FLOP;
    }

    if (strcmp(phase, "TURN") == 0)
    {
        return PHASE_TURN;
    }

    if (strcmp(phase, "RIVER") == 0)
    {
        return PHASE_RIVER;
    }

    if (strcmp(phase, "SHOWDOWN") == 0)
    {
        return PHASE_SHOWDOWN;
    }

    if (strcmp(phase, "GAME_OVER") == 0)
    {
        return PHASE_GAME_OVER;
    }

    /* If the phase string is unknown, return LOBBY safely. */
    return PHASE_LOBBY;
}

/*
 * Converts an ability string from the server into an AbilityType enum.
 *
 * Example:
 * "SNIFF" becomes ABILITY_SNIFF.
 */
static AbilityType ability_from_string(const char *ability)
{
    /* If ability is NULL, use ABILITY_NONE as a safe default. */
    if (ability == NULL)
    {
        return ABILITY_NONE;
    }

    /* Compare the server text against each known ability. */
    if (strcmp(ability, "NONE") == 0)
    {
        return ABILITY_NONE;
    }

    if (strcmp(ability, "SNIFF") == 0)
    {
        return ABILITY_SNIFF;
    }

    if (strcmp(ability, "ANT_TRAIL") == 0)
    {
        return ABILITY_ANT_TRAIL;
    }

    if (strcmp(ability, "POSE") == 0)
    {
        return ABILITY_POSE;
    }

    if (strcmp(ability, "LONG_TONGUE") == 0)
    {
        return ABILITY_LONG_TONGUE;
    }

    if (strcmp(ability, "WILD_GRAB") == 0)
    {
        return ABILITY_WILD_GRAB;
    }

    /* If the ability string is unknown, return no ability safely. */
    return ABILITY_NONE;
}

static void update_player_widgets_from_state(ClientState *client, const char *player_state)
{
    char copy[512];
    int opponent_slot = 0;
    char *saveptr = NULL;

    if (client == NULL || player_state == NULL)
    {
        return;
    }

    snprintf(copy, sizeof(copy), "%s", player_state);
    poker_gui_clear_opponents();

    /*
     * player_state is public server data:
     * seat|name|points|bet|status,seat|name|points|bet|status
     */
    char *entry = strtok_r(copy, ",", &saveptr);
    while (entry != NULL)
    {
        int seat = -1;
        int points = 0;
        int bet = 0;
        int status = PLAYER_EMPTY;
        char name[MAX_NAME_LEN];

        name[0] = '\0';

        if (sscanf(entry, "%d|%31[^|]|%d|%d|%d",
                   &seat, name, &points, &bet, &status) == 5)
        {
            if (seat == client->seat)
            {
                poker_gui_set_stack(points);
            }
            else if (opponent_slot < CLIENT_MAX_PLAYERS - 1)
            {
                char info[64];
                snprintf(info, sizeof(info), "Pts %d  Bet %d", points, bet);
                poker_gui_update_slot(opponent_slot, seat, name, info,
                                      seat == client->current_turn,
                                      status == PLAYER_FOLDED);
                opponent_slot++;
            }
        }

        entry = strtok_r(NULL, ",", &saveptr);
    }
}

/*
 * Parses command-line arguments for the client.
 *
 * Supported options:
 * --host <server_host>
 * --port <port_number>
 * --name <player_name>
 */
static void parse_client_args(
    int argc,
    char *argv[],
    char *host,
    int host_size,
    int *port,
    char *name,
    int name_size)
{
    /* Set default host, name, and port first. */
    snprintf(host, host_size, "%s", DEFAULT_HOST);
    snprintf(name, name_size, "Player");
    *port = DEFAULT_PORT;

    /* Walk through every command-line argument. */
    for (int i = 1; i < argc; i++)
    {
        /* Read the server host. */
        if ((strcmp(argv[i], "--host") == 0 || strstr(argv[i], "host") != NULL) && i + 1 < argc)
        {
            snprintf(host, host_size, "%s", argv[++i]);
        }

        /* Read the server port. */
        else if ((strcmp(argv[i], "--port") == 0 || strstr(argv[i], "port") != NULL) && i + 1 < argc)
        {
            *port = atoi(argv[++i]);
        }

        /* Read the player display name. */
        else if ((strcmp(argv[i], "--name") == 0 || strstr(argv[i], "name") != NULL) && i + 1 < argc)
        {
            snprintf(name, name_size, "%s", argv[++i]);
        }
    }
}

/*
 * Parses a STAT message from the server and updates ClientState.
 *
 * Expected format:
 * STAT:-1:phase=PREFLOP;players=1;pot=100;turn=0;community=0
 */
static void parse_stat_message(ClientState *client, const char *message)
{
    /* Buffer for the phase text. */
    char phase_text[64];

    /* Variables for values sent by the server. */
    int players = 0;
    int pot = 0;
    int turn = -1;
    int winner_seat = -1;
    char winner_name[CLIENT_NAME_LEN] = "";
    int community = 0;

    /* Make sure inputs are valid. */
    if (client == NULL || message == NULL)
    {
        return;
    }

    int matched = sscanf(
        message,
        "STAT:-1:phase=%63[^;];players=%d;pot=%d;turn=%d;winner=%d;community=%d",
        phase_text, &players, &pot, &turn, &winner_seat, &community);

    if (matched == 6)
    {
        client->phase = phase_from_string(phase_text);
        client->player_count = players;
        client->pot = pot;
        client->current_turn = turn;
        client->community_count = community;

        // parse optional community card list
        const char *p = strstr(message, "community_cards=");
        if (p) {
            p += strlen("community_cards=");
            char tmp[512];
            // copy until newline or end
            int i = 0;
            const char *end = strchr(p, ';');
            if (end == NULL)
                end = strchr(p, '\n');
            int len = end ? (int)(end - p) : (int)strlen(p);
            if (len >= (int)sizeof(tmp)) len = sizeof(tmp) - 1;
            memcpy(tmp, p, len);
            tmp[len] = '\0';

            char *card_saveptr = NULL;
            char *tok = strtok_r(tmp, ",", &card_saveptr);
            while (tok && i < COMMUNITY_CARD_SIZE) {
                // trim whitespace if needed
                while (*tok == ' ') tok++;
                Card c;
                if (string_to_card(tok, &c)) {
                    client->community_cards[i] = c;
                    // show image in GUI
                    char asset_path[128];
                    build_card_asset_path(asset_path, sizeof asset_path, tok); // token like "ace_of_spades" -> src/assets/ace_of_spades.png
                    poker_gui_set_community_card(i, asset_path);
                    i++;
                }
                tok = strtok_r(NULL, ",", &card_saveptr);
            }
            client->community_count = i;
        }

        const char *players_text = strstr(message, "player_state=");
        if (players_text)
        {
            char player_tmp[512];
            const char *end = strchr(players_text, '\n');
            int len;

            players_text += strlen("player_state=");
            len = end ? (int)(end - players_text) : (int)strlen(players_text);
            if (len >= (int)sizeof(player_tmp))
                len = sizeof(player_tmp) - 1;
            memcpy(player_tmp, players_text, len);
            player_tmp[len] = '\0';
            update_player_widgets_from_state(client, player_tmp);

            /* If server included a winner seat, try to find the player's name. */
            if (winner_seat >= 0) {
                char copy[512];
                char *saveptr = NULL;
                snprintf(copy, sizeof(copy), "%s", player_tmp);
                char *entry = strtok_r(copy, ",", &saveptr);
                while (entry != NULL) {
                    int seat = -1;
                    char name[MAX_NAME_LEN];
                    int points = 0, bet = 0, status = 0;
                    name[0] = '\0';
                    if (sscanf(entry, "%d|%31[^|]|%d|%d|%d",
                               &seat, name, &points, &bet, &status) == 5)
                    {
                        if (seat == winner_seat) {
                            strncpy(winner_name, name, CLIENT_NAME_LEN - 1);
                            winner_name[CLIENT_NAME_LEN - 1] = '\0';
                            break;
                        }
                    }
                    entry = strtok_r(NULL, ",", &saveptr);
                }
            }
        }

        // update GUI pot
        poker_gui_set_pot(pot);

        // Highlight local avatar if this client is active
        poker_gui_set_my_turn_active(turn >= 0 && turn == client->seat);

        /* update GUI status — handle game-over separately */
        if (client->phase == PHASE_GAME_OVER)
        {
            poker_gui_set_my_turn_active(0);
            poker_gui_set_status("Hand over");
            if (winner_seat >= 0) {
                char winner_msg[128];
                if (winner_name[0])
                    snprintf(winner_msg, sizeof winner_msg, "%s - Seat %d won the hand!", winner_name, winner_seat + 1);
                else
                    snprintf(winner_msg, sizeof winner_msg, "Seat %d won the hand!", winner_seat + 1);
                poker_gui_set_winner(winner_msg);
            } else {
                poker_gui_set_winner("");
            }
        }
        else if (client->phase == PHASE_LOBBY || turn < 0)
        {
            poker_gui_set_my_turn_active(0);
            poker_gui_set_status("Waiting for hand to begin...");
            poker_gui_set_winner("");
        }
        else if (turn == client->seat)
        {
            poker_gui_set_my_turn_active(1);
            poker_gui_set_status("Your turn");
            poker_gui_set_winner("");
        }
        else
        {
            poker_gui_set_my_turn_active(0);
            char buf[64];
            snprintf(buf, sizeof buf, "Waiting for seat %d...", turn + 1);
            poker_gui_set_status(buf);
            poker_gui_set_winner("");
        }

        set_client_status(client, "Received public game state.");
    }
    else
    {
        poker_gui_set_status("Could not parse game state.");
    }
}

/*
 * Parses a HAND message from the server and updates ClientState.
 *
 * Expected format:
 * HAND:0:Jack of Diamonds,Ace of Clubs,ability=SNIFF
 *
 * Stores the ability and updates the GUI card images.
 */
static void parse_hand_message(ClientState *client, const char *message)
{
    /* Stores the seat number from the HAND message. */
    int seat = -1;

    /* Stores the two card strings as text. */
    char card1_text[128];
    char card2_text[128];

    /* Stores the ability string from the server. */
    char ability_text[64];
    int points = 0;

    /* Make sure inputs are valid. */
    if (client == NULL || message == NULL)
    {
        return;
    }

    /*
     * Parse the HAND message.
     *
     * %127[^,] reads everything until the first comma.
     * %127[^,] reads everything until the second comma.
     * %63s reads the ability text after ability=.
     */
    int matched = sscanf(
        message,
        "HAND:%d:%127[^,],%127[^,],ability=%63[^;];points=%d",
        &seat,
        card1_text,
        card2_text,
        ability_text,
        &points);

    /*
     * If parsing worked, update the client's ability and status.
     */
    if (matched >= 4)
    {
        client->seat = seat;
        client->ability = ability_from_string(ability_text);

        // build asset paths and show cards in GUI
        char path1[128], path2[128];
        build_card_asset_path(path1, sizeof path1, card1_text);
        build_card_asset_path(path2, sizeof path2, card2_text);
        poker_gui_set_my_card(0, path1);
        poker_gui_set_my_card(1, path2);

        // update stack display using the server-authoritative point total
        if (matched == 5)
            poker_gui_set_stack(points);

        char ability_label[96];
        snprintf(ability_label, sizeof ability_label, "Ability: %s", ability_text);
        poker_gui_set_ability(ability_label);

        set_client_status(client, "Received private hand.");

        if (client->phase == PHASE_LOBBY || client->current_turn < 0)
        {
            poker_gui_set_my_turn_active(0);
            poker_gui_set_status("Waiting for hand to begin...");
        }
        else if (client->current_turn == client->seat)
        {
            poker_gui_set_my_turn_active(1);
            poker_gui_set_status("Your turn!");
        }
    }
    else
    {
        poker_gui_set_status("Could not parse private hand.");
    }
}

/*
 * Handles one complete server message line.
 *
 * Example lines:
 * INFO:-1:Connected. Send LOGIN:-1:your_name
 * SEAT:0:Welcome Hamza
 * STAT:-1:phase=PREFLOP;players=1;pot=0;turn=0;community=0
 * HAND:0:Jack of Diamonds,Ace of Clubs,ability=SNIFF
 * OK:-1:Check accepted
 * ERROR:-1:Not your turn
 */
static void handle_single_server_message(ClientState *client, const char *message)
{
    /* Make sure inputs are valid. */
    if (client == NULL || message == NULL || message[0] == '\0')
    {
        return;
    }

    /* Print the raw server message for debugging. */
    printf("Server says: %s\n", message);

    /*
     * SEAT message means the server assigned this client a seat.
     */
    if (strncmp(message, "SEAT:", 5) == 0)
    {
        int seat = -1;

        /* Extract the seat number. */
        if (sscanf(message, "SEAT:%d:", &seat) == 1)
        {
            client->seat = seat;
            set_client_status(client, "Logged in and seated.");
            poker_gui_set_my_seat(seat);
            char buf[64];
            snprintf(buf, sizeof buf, "Seated at seat %d.", seat + 1);
            poker_gui_set_status(buf);
        }
    }

    /*
     * STAT message contains public game state.
     */
    else if (strncmp(message, "STAT:", 5) == 0)
    {
        parse_stat_message(client, message);
    }

    /*
     * HAND message contains the private hand and ability.
     */
    else if (strncmp(message, "HAND:", 5) == 0)
    {
        parse_hand_message(client, message);
    }

    /*
     * ERROR message means the server rejected an action.
     */
    else if (strncmp(message, "ERROR:", 6) == 0)
    {
        const char *payload = strchr(message + 6, ':');
        if (payload)
            payload++;
        poker_gui_set_status(payload ? payload : "Server error.");
        set_client_status(client, "Server returned an error.");
    }

    /*
     * OK message means the server accepted an action.
     */
    else if (strncmp(message, "OK:", 3) == 0)
    {
        set_client_status(client, "Action accepted.");
    }

    /*
     * ABIL messages contain the private result of an Anteater ability.
     * Sniff, for example, can reveal a card only to the player who used it.
     */
    else if (strncmp(message, "ABIL:", 5) == 0)
    {
        const char *payload = strchr(message + 5, ':');
        if (payload)
            payload++;
        poker_gui_set_alert(payload ? payload : "Ability resolved.");
        set_client_status(client, "Ability resolved.");
    }

    /*
     * INFO message is just informational.
     */
    else if (strncmp(message, "INFO:", 5) == 0)
    {
        set_client_status(client, "Server sent information.");
    }
}

/*
 * Handles a server buffer that may contain multiple messages.
 *
 * TCP can combine messages together.
 * For example, one read may contain:
 *
 * STAT:-1:phase=PREFLOP;players=1;pot=0;turn=0;community=0
 * HAND:0:Jack of Diamonds,Ace of Clubs,ability=SNIFF
 *
 * This function splits the buffer by newline and handles each message separately.
 */
static void handle_server_buffer(ClientState *client, const char *buffer)
{
    /* Local copy because strtok modifies the string. */
    char copy[CLIENT_BUFFER_SIZE];
    char *saveptr = NULL;

    /* Make sure inputs are valid. */
    if (client == NULL || buffer == NULL)
    {
        return;
    }

    /* Copy the server buffer safely. */
    snprintf(copy, sizeof(copy), "%s", buffer);

    /* Split the buffer into lines using newline as the delimiter. */
    char *line = strtok_r(copy, "\n", &saveptr);

    /* Process every complete line. */
    while (line != NULL)
    {
        handle_single_server_message(client, line);
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

/*
 * on_server_readable
 *
 * GTK calls this whenever the server socket has data.
 */
static gboolean on_server_readable(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    (void)channel;
    (void)cond;
    (void)data;

    char buffer[CLIENT_BUFFER_SIZE];
    memset(buffer, 0, sizeof buffer);

    int bytes = receive_from_server(g_client.socket_fd, buffer, sizeof buffer);

    if (bytes <= 0)
    {
        printf("Server disconnected.\n");
        poker_gui_set_status("Disconnected from server.");
        close(g_client.socket_fd);
        g_client.connected = 0;
        return FALSE; // stop watching
    }

    handle_server_buffer(&g_client, buffer);
    return TRUE; // keep watching
}

/*
 * main
 *
 * 1. Init GTK.
 * 2. Parse args, connect to server.
 * 3. Send LOGIN.
 * 4. Register socket with g_io_add_watch.
 * 5. Launch poker GUI window.
 * 6. gtk_main() handles everything from here.
 */
int main(int argc, char *argv[])
{
    char host[128];
    char name[CLIENT_NAME_LEN];
    int port;

    init_client_state(&g_client);
    parse_client_args(argc, argv, host, sizeof host, &port, name, sizeof name);
    set_client_name(&g_client, name);

    /*
     * Parse poker options before GTK initialization. GTK consumes options like
     * --name for its own resource naming, which would otherwise hide the
     * player's display name from our client parser.
     */
    if (!gtk_init_check(&argc, &argv))
    {
        fprintf(stderr, "Failed to initialize GTK. Check DISPLAY/WSLg or run from a graphical session.\n");
        return 1;
    }

    printf("Connecting to %s:%d as %s...\n", host, port, name);

    g_client.socket_fd = connect_to_server(host, port);
    if (g_client.socket_fd < 0)
    {
        fprintf(stderr, "Could not connect to server.\n");
        return 1;
    }

    g_client.connected = 1;

    // receive initial INFO message from server
    char buffer[CLIENT_BUFFER_SIZE];
    memset(buffer, 0, sizeof buffer);
    int bytes = receive_from_server(g_client.socket_fd, buffer, sizeof buffer);
    if (bytes > 0)
        handle_server_buffer(&g_client, buffer);

    // send LOGIN
    char login_msg[CLIENT_BUFFER_SIZE];
    snprintf(login_msg, sizeof login_msg, "LOGIN:-1:%s\n", name);
    send_to_server(g_client.socket_fd, login_msg);

    // register socket with GTK so on_server_readable fires on incoming data
    GIOChannel *channel = g_io_channel_unix_new(g_client.socket_fd);
    g_io_add_watch(channel, G_IO_IN, on_server_readable, NULL);
    g_io_channel_unref(channel);

    // launch GUI — passes socket fd so button callbacks can send to server
    launch_poker_window(g_client.socket_fd);

    /* Show our player name */
    poker_gui_set_name(g_client.player_name);
    poker_gui_set_my_seat(g_client.seat);

    gtk_main();

    close(g_client.socket_fd);
    return 0;
}
