#include "game_state.h"
#include "board.h"
#include <stdlib.h>

int running;
int key;
int auto_on;

GameState current_state;

void init_ncurses(){
	initscr();
	keypad(stdscr,1);
	noecho();
	curs_set(0);
	start_color();
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
}

void init_state(){
	start = start_frame = end = (struct timespec){0};
	running = 1;
	key = 0;
	auto_on = 0;
}

void start_round(){
	srand(seed);
	init_state();
	current_state = STATE_PLAYING;
	board = 0;
	score = 0;
	natural_4s=0;
	add_random();
	depth_reached = 0;
}



void end_ncurses(){
	endwin();
}

void end_game(){
	running = 0;
	end_ncurses();
	exit(0);
}
