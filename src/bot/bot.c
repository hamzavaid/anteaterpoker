#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "bot.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Basic command-line arguments for connection
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
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

    //Send the login message to the server
    const char *login_msg = "LOGIN:-1:AnteaterBot\n";
    send(sock_fd, login_msg, strlen(login_msg), 0);

    // Initialize the Bot's local memory
    BotState state;
    memset(&state, 0, sizeof(BotState));
    state.my_seat = -1; // Will be set when server confirms login

    // 5. The Main Network Loop
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
            
            // --- PARSE AND UPDATE MEMORY ---
            // Pass every message to your parser to keep the bot's memory accurate
            bot_update_state(&state, line);

            // --- CHECK IF IT IS THE BOT'S TURN ---
            if (strncmp(line, "STAT", 4) == 0) {
                
                
                // --- CHECK IF IT IS THE BOT'S TURN ---
                if (strncmp(line, "STAT", 4) == 0) {
                    if (state.is_my_turn) {
                    printf("Bot analyzing board...\n");
                 
                    // Ask the AI what to do
                    const char *decision = bot_decide_action(&state);
                     
                    // Format the response and send it
                    char response[64];
                    snprintf(response, sizeof(response), "ACTN:-1:%s\n", decision);
                     
                    printf("Bot sends: %s", response);
                    send(sock_fd, response, strlen(response), 0);

                    // Reset turn flag so we don't spam the server multiple times
                    state.is_my_turn = 0; 
                 }
            }
            }

            line = strtok(NULL, "\n");
        }
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}
