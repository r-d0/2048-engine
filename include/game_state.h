#pragma once
#include "board.h"

extern int key;
extern int running;
extern int auto_on;

typedef enum{
	STATE_MENU,
	STATE_PLAYING,
	STATE_GAMEOVER,
	STATE_SETTINGS,
}GameState;

extern GameState current_state;

extern void init_ncurses();
extern void init_state();
extern void start_round();
extern void end_ncurses();
extern void end_game();
