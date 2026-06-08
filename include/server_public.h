#ifndef SERVER_PUBLIC_H
#define SERVER_PUBLIC_H

#include "game_state.h"

void send_public_state_to_all(const GameState *game);
void send_private_hands_to_all(GameState *game);

#endif
