#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#include "bot.h"

#define BUFFER_SIZE 1024

// Global variable for the socket so the signal handler can reach it
int global_sock_fd; 

// Signal handler for clean exit on Ctrl+C
void handle_sigint(int sig) {
    (void)sig; // Unused parameter
    printf("\nShutdown detected . Leaving game...\n");
    
    // Tell the server bot is leaving so seat can be cleared
    const char *leave_msg = "LEAVE:-1\n"; 
    send(global_sock_fd, leave_msg, strlen(leave_msg), 0);
    
    close(global_sock_fd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // Hook up the signal handler
    signal(SIGINT, handle_sigint);

    // Basic command-line arguments for connection
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <port> <bot_name>\n", argv[0]);
    return EXIT_FAILURE;
}

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    // Set up the socket connection
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    
    // Save the socket to the global variable so the handler can use it
    global_sock_fd = sock_fd;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("Bot successfully connected to %s:%d\n", server_ip, port);

    /* Seed random behavior once per run so the bot can sometimes bluff. */
    BotState state;
    srand((unsigned)(time(NULL) ^ (uintptr_t)&state));

    //Send the login message to the server
    char login_msg[256];
    snprintf(login_msg, sizeof(login_msg), "LOGIN:-1:%s\n", argv[3]);
    send(sock_fd, login_msg, strlen(login_msg), 0);

    // Initialize the Bot's local memory
    memset(&state, 0, sizeof(BotState));
    state.my_seat = -1; // Will be set when server confirms login

    // The Main Network Loop
    char buffer[BUFFER_SIZE];
    
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Server disconnected. Bot shutting down.\n");
            break;
        }

        // Messages from the server might be batched together, separated by newlines.
        // We need to process them one line at a time.
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            
            // Check for server-initiated kick first to avoid processing any more messages after that
            if (strncmp(line, "KICK", 4) == 0) {
                printf("\nServer initiated kick. Shutting down gracefully...\n");
                close(sock_fd);
                return EXIT_SUCCESS; 
            }

            // Pass every message to your parser to keep the bot's memory accurate
            bot_update_state(&state, line);

            //Check if it is the bot's turn 
            if (strncmp(line, "STAT", 4) == 0) {
                // Check if it is bot turn
                if (state.is_my_turn) {
                    printf("Bot analyzing board...\n");
                 
                    // Ask bot what to do
                    const char *decision = bot_decide_action(&state);

                    // Format the response and send it. If raising, include amount.
                    char response[128];
                    if (decision && strcmp(decision, "RAISE") == 0) {
                        int amount = bot_calculate_raise(&state);
                        if (amount <= 0) {
                            // Fallback to CHECK if raise calculation failed
                            snprintf(response, sizeof(response), "ACTN:-1:CHECK\n");
                        } else {
                            snprintf(response, sizeof(response), "RAISE:-1:%d\n", amount);
                        }
                    } else {
                        snprintf(response, sizeof(response), "ACTN:-1:%s\n", decision ? decision : "CHECK");
                    }

                    printf("Bot sends: %s", response);
                    send(sock_fd, response, strlen(response), 0);

                    // Reset turn flag so we don't spam the server multiple times
                    state.is_my_turn = 0; 
                }
            }

            line = strtok(NULL, "\n");
        }
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}