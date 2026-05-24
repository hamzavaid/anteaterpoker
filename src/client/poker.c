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

#include <gtk/gtk.h>

#include "client_state.h"
#include "socket_client.h"
#include "poker_gui.h"

/* Default server host if the user does not provide --host. */
#define DEFAULT_HOST "localhost"

/* Default project port. */
#define DEFAULT_PORT 10010

static ClientState g_client;

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
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc)
        {
            snprintf(host, host_size, "%s", argv[++i]);
        }

        /* Read the server port. */
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            *port = atoi(argv[++i]);
        }

        /* Read the player display name. */
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc)
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
    int community = 0;

    /* Make sure inputs are valid. */
    if (client == NULL || message == NULL)
    {
        return;
    }

    int matched = sscanf(
        message,
        "STAT:-1:phase=%63[^;];players=%d;pot=%d;turn=%d;community=%d",
        phase_text, &players, &pot, &turn, &community);
 
    if (matched == 5)
    {
        client->phase          = phase_from_string(phase_text);
        client->player_count   = players;
        client->pot            = pot;
        client->current_turn   = turn;
        client->community_count = community;
 
        // update GUI pot
        poker_gui_set_pot(pot);
 
        // update GUI status — show whose turn it is
        if (turn == client->seat)
            poker_gui_set_status("Your turn!");
        else
        {
            char buf[64];
            snprintf(buf, sizeof buf, "Waiting for seat %d...", turn);
            poker_gui_set_status(buf);
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
 * This starter parser stores the ability.
 * It prints the cards, but does not fully convert card strings back into Card structs yet.
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
        "HAND:%d:%127[^,],%127[^,],ability=%63s",
        &seat,
        card1_text,
        card2_text,
        ability_text);

    /*
     * If parsing worked, update the client's ability and status.
     */
     if (matched == 4)
    {
        client->seat    = seat;
        client->ability = ability_from_string(ability_text);
 
        // build asset paths and show cards in GUI
        char path1[128], path2[128];
        snprintf(path1, sizeof path1, "assets/%s.png", card1_text);
        snprintf(path2, sizeof path2, "assets/%s.png", card2_text);
        poker_gui_set_my_card(0, path1);
        poker_gui_set_my_card(1, path2);
 
        // update stack display
        poker_gui_set_stack(client->pot);
 
        set_client_status(client, "Received private hand.");
        poker_gui_set_status("Cards dealt. Good luck!");
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
            char buf[64];
            snprintf(buf, sizeof buf, "Seated at seat %d.", seat);
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
        if (payload) payload++;
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

    /* Make sure inputs are valid. */
    if (client == NULL || buffer == NULL)
    {
        return;
    }

    /* Copy the server buffer safely. */
    snprintf(copy, sizeof(copy), "%s", buffer);

    /* Split the buffer into lines using newline as the delimiter. */
    char *line = strtok(copy, "\n");

    /* Process every complete line. */
    while (line != NULL)
    {
        handle_single_server_message(client, line);
        line = strtok(NULL, "\n");
    }
}

/*
 * on_server_readable
 *
 * GTK calls this whenever the server socket has data.
 */
static gboolean on_server_readable(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    (void)cond; (void)data;
 
    char buffer[CLIENT_BUFFER_SIZE];
    memset(buffer, 0, sizeof buffer);
 
    int bytes = receive_from_server(g_client.socket_fd, buffer, sizeof buffer);
 
    if (bytes <= 0)
    {
        printf("Server disconnected.\n");
        poker_gui_set_status("Disconnected from server.");
        close(g_client.socket_fd);
        g_client.connected = 0;
        g_io_channel_unref(channel);
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
    gtk_init(&argc, &argv);
 
    char host[128];
    char name[CLIENT_NAME_LEN];
    int  port;
 
    init_client_state(&g_client);
    parse_client_args(argc, argv, host, sizeof host, &port, name, sizeof name);
    set_client_name(&g_client, name);
 
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
 
    gtk_main();
 
    close(g_client.socket_fd);
    return 0;
}
