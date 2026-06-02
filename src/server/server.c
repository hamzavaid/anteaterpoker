/*
 * server.c
 *
 * Main server program for Anteater Poker.
 *
 * Responsibilities:
 *   - Read command-line options such as --port and --table.
 *   - Start the TCP socket server.
 *   - Accept client connections.
 *   - Receive client messages.
 *   - Update GameState by calling game-state helper functions.
 *   - Send public table updates and private hand messages.
 *
 * The alpha server supports login, hand start, simple actions, raises,
 * disconnects, and public/private messages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>
#include <gtk/gtk.h>

#include "game_state.h"
#include "poker_rules.h"
#include "anteater_abilities.h"
#include "socket_server.h"
#include "server_gui.h"
#include "server_public.h"
#include "bot.h"

// global variable for GTK
static GameState g_game;

/*
 * parse_server_args
 *
 * Reads command-line arguments and updates the ServerConfig.
 *
 * Supported examples:
 *   ./bin/poker_server --port 10010 --table "ZotHouse"
 *   ./bin/poker_server --points 1000 --bots 1 --log logs/game.log
 *
 * This function is static because it is only used inside server.c.
 */
static void parse_server_args(int argc, char *argv[], ServerConfig *config)
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            config->port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--table") == 0 && i + 1 < argc)
        {
            snprintf(config->table_name, MAX_TABLE_NAME_LEN, "%s", argv[++i]);
        }
        else if (strcmp(argv[i], "--points") == 0 && i + 1 < argc)
        {
            config->starting_points = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--bots") == 0 && i + 1 < argc)
        {
            config->bot_count = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc)
        {
            snprintf(config->log_path, MAX_LOG_PATH_LEN, "%s", argv[++i]);
        }
    }
}

/*
 * add_startup_bots
 *
 * Creates the in-process bot players requested by --bots.
 * Bot players use socket_fd = -1 and are driven by server_auto_play_bot_turns.
 */
static void add_startup_bots(GameState *game)
{
    int requested;

    if (game == NULL) {
        return;
    }

    requested = game->config.bot_count;
    if (requested < 0) {
        requested = 0;
    }
    if (requested > MAX_PLAYERS) {
        requested = MAX_PLAYERS;
    }

    game->config.bot_count = 0;

    for (int i = 0; i < requested; i++) {
        char bot_name[MAX_NAME_LEN];
        int seat;

        snprintf(bot_name, sizeof bot_name, "Bot %d", game->config.bot_count + 1);
        seat = add_player(game, -1, bot_name);
        if (seat < 0) {
            fprintf(stderr, "[SERVER] Could only add %d startup bot(s); table is full.\n",
                    game->config.bot_count);
            break;
        }

        game->config.bot_count++;
        printf("[SERVER] Bot '%s' added to seat %d from --bots\n", bot_name, seat);
    }
}

/*
 * collect_client_fds
 *
 * Builds an array of active client socket file descriptors.
 * This is used when broadcasting a message to all connected clients.
 *
 * Parameters:
 *   game        - current server game state
 *   fds         - output array to store socket descriptors
 *   num_clients - output count of connected client sockets
 */
static void collect_client_fds(const GameState *game, int fds[], int *num_clients)
{
    *num_clients = 0;

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (game->players[i].status != PLAYER_EMPTY &&
            strcmp(game->players[i].name, "Guest") != 0 &&
            game->players[i].socket_fd >= 0)
        {
            fds[*num_clients] = game->players[i].socket_fd;
            (*num_clients)++;
        }
    }
}

/*
 * find_seat_by_socket
 *
 * Finds which player seat belongs to a given socket descriptor.
 *
 * Returns:
 *   seat number if found, otherwise -1.
 */
static int find_seat_by_socket(const GameState *game, int socket_fd)
{
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (game->players[i].socket_fd == socket_fd &&
            game->players[i].status != PLAYER_EMPTY)
        {
            return i;
        }
    }

    return -1;
}

/*
 * send_public_state_to_all
 *
 * Creates a public GAME_STATE/STAT message and broadcasts it to all clients.
 * This message does not include private cards.
 */
void send_public_state_to_all(const GameState *game)
{
    int fds[MAX_PLAYERS];
    int num_clients = 0;
    char state_msg[MESSAGE_BUFFER_SIZE];

    collect_client_fds(game, fds, &num_clients);
    build_public_game_state(game, state_msg, sizeof(state_msg));
    broadcast_game(fds, num_clients, state_msg);
}

/*
 * send_private_hands
 *
 * Sends each active player their own private hand message.
 * Each private hand must only be sent to that player's socket.
 */
void send_private_hands_to_all(GameState *game)
{
    char hand_msg[MESSAGE_BUFFER_SIZE];

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (game->players[i].status == PLAYER_ACTIVE &&
            game->players[i].socket_fd >= 0)
        {
            build_private_hand_message(game, i, hand_msg, sizeof(hand_msg));
            send_message(game->players[i].socket_fd, hand_msg);
        }
    }
}

/*
 * server_auto_play_bot_turns
 *
 * Checks if the current turn is a bot player (socket_fd == -1).
 * If so, calls the bot AI to decide an action and applies it.
 * Loops until a human player's turn or hand ends.
 */
static void server_auto_play_bot_turns(GameState *game)
{
    if (game == NULL || game->current_turn < 0) {
        return;
    }

    while (game->current_turn >= 0 && game->current_turn < MAX_PLAYERS) {
        Player *player = &game->players[game->current_turn];

        /* Check if this seat is a bot (socket_fd == -1) */
        if (player->status != PLAYER_ACTIVE || player->socket_fd >= 0) {
            break; /* Human player or empty seat */
        }

        /* Build a bot state from game state */
        BotState bot_state;
        memset(&bot_state, 0, sizeof(BotState));
        bot_state.my_seat = game->current_turn;
        bot_state.points = player->points;
        bot_state.current_bet = player->current_bet;
        bot_state.table_highest_bet = game->current_bet;
        bot_state.pot = game->pot;
        bot_state.is_my_turn = 1;

        /* Copy private hand */
        for (int i = 0; i < PRIVATE_HAND_SIZE && i < 2; i++) {
            bot_state.my_cards[i] = player->hand[i];
        }

        /* Copy community cards */
        for (int i = 0; i < game->community_count && i < COMMUNITY_CARD_SIZE; i++) {
            bot_state.community_cards[i] = game->community_cards[i];
        }
        bot_state.community_count = game->community_count;

        /* Get bot decision */
        const char *decision = bot_decide_action(&bot_state);
        if (decision == NULL) {
            decision = "CHECK";
        }

        printf("[BOT] Seat %d decides: %s\n", game->current_turn, decision);

        /* Apply the action */
        if (strcmp(decision, "RAISE") == 0) {
            int raise_amount = bot_calculate_raise(&bot_state);
            if (raise_amount > 0) {
                poker_apply_action(game, game->current_turn, "RAISE", raise_amount);
            } else {
                poker_apply_action(game, game->current_turn, "CHECK", 0);
            }
        } else {
            poker_apply_action(game, game->current_turn, decision, 0);
        }

        /* Update GUI after bot action */
        send_public_state_to_all(game);
        send_private_hands_to_all(game);
        server_gui_refresh();
    }
}

/*
 * handle_client_message
 *
 * Processes one message received from a client.
 *
 * Supported commands:
 *   LOGIN:-1:<name>     - add a player to the table
 *   START:-1:anything   - start a new hand
 *   ACTN:<seat>:CHECK   - check
 *   ACTN:<seat>:CALL    - call
 *   ACTN:<seat>:FOLD    - fold
 *   RAISE:<seat>:50     - raise by 50 points
 *   QUIT:<seat>:bye     - leave table
 *
 * The server remains authoritative. Clients request actions, but the server
 * decides whether to accept or reject them.
 */
static void handle_client_message(GameState *game, int client_fd, const char *buf)
{
    /* Convert the raw text message into a NetworkMessage struct. */
    NetworkMessage msg = parse_client_msg(buf);

    printf("Received: command=%s player=%d payload=%s\n",
           msg.command,
           msg.player_id,
           msg.payload);

    /*
     * LOGIN command:
     * Assigns the client to an empty seat and stores their name.
     */
    if (strcmp(msg.command, "LOGIN") == 0)
    {
        int seat = find_seat_by_socket(game, client_fd);

        if (seat < 0)
        {
            send_message(client_fd, "ERROR:-1:Player not connected\n");
            return;
        }

        snprintf(game->players[seat].name, MAX_NAME_LEN, "%.*s",
                 MAX_NAME_LEN - 1, msg.payload);
        game->players[seat].status = PLAYER_CONNECTED;

        char reply[MESSAGE_BUFFER_SIZE];
        snprintf(reply, sizeof(reply), "SEAT:%d:Welcome %s\n", seat, msg.payload);

        send_message(client_fd, reply);
        send_public_state_to_all(game);
        server_gui_refresh();
        return;
    }

    /*
     * START command:
     * Starts a new hand, broadcasts public state, then sends private hands.
     */
    if (strcmp(msg.command, "START") == 0)
    {
        start_new_hand(game);
        send_public_state_to_all(game);
        send_private_hands_to_all(game);
        server_gui_refresh();
        server_auto_play_bot_turns(game);
        return;
    }

    /*
     * ACTN command:
     * Handles simple player actions. Current version supports CHECK, CALL,
     * and FOLD. Shared legality helpers live in poker_rules.c.
     */
    if (strcmp(msg.command, "ACTN") == 0)
    {
        int seat = find_seat_by_socket(game, client_fd);

        if (seat < 0)
        {
            send_message(client_fd, "ERROR:-1:Player not logged in\n");
            return;
        }

        /* Only the current-turn player may act. */
        if (seat != game->current_turn)
        {
            send_message(client_fd, "ERROR:-1:Not your turn\n");
            return;
        }

        if (!poker_apply_action(game, seat, msg.payload, 0))
        {
            send_message(client_fd, "ERROR:-1:Illegal action\n");
            return;
        }

        send_message(client_fd, "OK:-1:Action accepted\n");
        send_public_state_to_all(game);
        send_private_hands_to_all(game);
        server_gui_refresh();
        server_auto_play_bot_turns(game);
        return;
    }

    /*
     * RAISE command:
     * Adds points to the pot and updates the player's current bet.
     *
     * Current version only checks positive amount and available points.
     * Later, poker_rules.c should check minimum raise and betting-round rules.
     */
    if (strcmp(msg.command, "RAISE") == 0)
    {
        int seat = find_seat_by_socket(game, client_fd);
        int amount = atoi(msg.payload);

        if (seat < 0)
        {
            send_message(client_fd, "ERROR:-1:Player not logged in\n");
            return;
        }

        if (!poker_apply_action(game, seat, "RAISE", amount))
        {
            send_message(client_fd, "ERROR:-1:Invalid raise amount\n");
            return;
        }

        send_message(client_fd, "OK:-1:Raise accepted\n");
        send_public_state_to_all(game);
        send_private_hands_to_all(game);
        server_gui_refresh();
        server_auto_play_bot_turns(game);
        return;
    }

    /*
     * ABIL command:
     * Uses the player's once-per-hand Anteater ability.
     * Payload format: <target_seat>[:param]
     */
    if (strcmp(msg.command, "ABIL") == 0)
    {
        int seat = find_seat_by_socket(game, client_fd);
        int target = -1;
        int param = 0;
        char result[256];
        char reply[MESSAGE_BUFFER_SIZE];

        if (seat < 0)
        {
            send_message(client_fd, "ERROR:-1:Player not logged in\n");
            return;
        }

        sscanf(msg.payload, "%d:%d", &target, &param);

        if (!anteater_apply_ability(game, seat, target, param, result, sizeof(result)))
        {
            snprintf(reply, sizeof(reply), "ERROR:-1:%s\n", result);
            send_message(client_fd, reply);
            return;
        }

        send_public_state_to_all(game);
        send_private_hands_to_all(game);
        snprintf(reply, sizeof(reply), "ABIL:%d:%s\n", seat, result);
        send_message(client_fd, reply);
        server_gui_refresh();
        return;
    }

    /*
     * QUIT command:
     * Removes the player from the table and closes their client socket.
     */
    if (strcmp(msg.command, "QUIT") == 0)
    {
        int seat = find_seat_by_socket(game, client_fd);

        if (seat >= 0)
        {
            remove_player(game, seat);
        }

        close(client_fd);
        send_public_state_to_all(game);
        server_gui_refresh();
        return;
    }

    /* If no command matched, reject the message. */
    send_message(client_fd, "ERROR:-1:Unknown command\n");
}

/*
 * on_client_readable
 *
 * GTK calls this when a connected client socket has data to read.
 *
 * Returns TRUE to keep watching the socket, FALSE to stop.
 * Returning FALSE also removes the watch automatically.
 */
static gboolean on_client_readable(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    (void)channel;
    (void)cond;
    int client_fd = GPOINTER_TO_INT(data);

    char buffer[MESSAGE_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0)
    {
        // client disconnected — find their seat and remove them
        printf("Client disconnected.\n");
        int seat = find_seat_by_socket(&g_game, client_fd);
        if (seat >= 0)
            remove_player(&g_game, seat);
        close(client_fd);
        send_public_state_to_all(&g_game);
        server_gui_refresh();

        return FALSE; // stop watching this socket
    }

    handle_client_message(&g_game, client_fd, buffer);

    if (find_seat_by_socket(&g_game, client_fd) < 0)
    {
        return FALSE;
    }

    return TRUE;
}

/*
 * on_server_readable
 *
 * GTK calls this when the listening socket has a new incoming connection.
 *
 * After accepting the client we register it with g_io_add_watch so
 * on_client_readable handles its future messages.
 */
static gboolean on_server_readable(GIOChannel *channel, GIOCondition cond, gpointer data)
{
    (void)channel;
    (void)cond;
    (void)data;

    int client_fd = accept_client(g_game.server_fd);

    if (client_fd < 0)
        return TRUE; // accept failed, keep listening

    int seat = add_player(&g_game, client_fd, "Guest");

    if (seat < 0)
    {
        send_message(client_fd, "ERROR:-1:Seat unavailable\n");
        close(client_fd);
        return TRUE;
    }

    send_message(client_fd, "INFO:-1:Connected. Send LOGIN:-1:your_name\n");

    // register this new client socket with GTK so on_client_readable
    // fires automatically whenever it sends data
    GIOChannel *client_channel = g_io_channel_unix_new(client_fd);
    g_io_add_watch(client_channel, G_IO_IN,
                   on_client_readable,
                   GINT_TO_POINTER(client_fd));
    g_io_channel_unref(client_channel);

    server_gui_refresh();
    return TRUE; // keep watching the listening socket
}

/*
 * main
 *
 * Program entry point for the server.
 *
 * Flow:
 *   1. Load default config.
 *   2. Parse command-line arguments.
 *   3. Open a listening socket.
 *   4. Initialize GameState.
 *   5. Register listening socket
 *   6. Launch server GUI and let gtk_main handle rest
 */
int main(int argc, char *argv[])
{
    if (!gtk_init_check(&argc, &argv))
    {
        fprintf(stderr, "Failed to initialize GTK. Check DISPLAY/WSLg or run from a graphical session.\n");
        return 1;
    }

    ServerConfig config;

    /* Load default options first, then override with command-line values. */
    init_server_config(&config);
    parse_server_args(argc, argv, &config);

    /* Create the server listening socket. */
    int server_fd = init_server(config.port);

    if (server_fd < 0)
    {
        fprintf(stderr, "Failed to start server.\n");
        return 1;
    }

    /* Initialize the official server-side game state. */
    init_game_state(&g_game, &config, server_fd);
    add_startup_bots(&g_game);

    printf("Anteater Poker server started.\n");
    printf("Table: %s\n", config.table_name);
    printf("Port: %d\n", config.port);
    printf("Waiting for clients...\n");

    // register the listening socket with the GTK event loop
    // GTK will call on_server_readable whenever a new client connects
    GIOChannel *server_channel = g_io_channel_unix_new(server_fd);
    g_io_add_watch(server_channel, G_IO_IN, on_server_readable, NULL);
    g_io_channel_unref(server_channel);

    // build and show the server GUI window
    // passes &g_game so the GUI can read state and call game functions
    launch_server_window(&g_game);

    gtk_main();

    close(server_fd);
    return 0;
}
