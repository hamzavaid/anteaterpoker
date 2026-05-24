#ifndef SERVER_GUI_H
#define SERVER_GUI_H
 
#include "game_state.h"
 
// call once after init_game_state() to build and show the server window
void launch_server_window(GameState *game);
 
// call after any game state change to update all widgets on screen
void server_gui_refresh(void);
 
#endif
