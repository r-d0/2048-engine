#include "game_state.h"
#include "board.h"
#include <stdlib.h>

int running;
int key;

void init_ncurses(){
	initscr();
	keypad(stdscr,1);
	noecho();
	curs_set(0);
}

void init_game(){
	running = 1;
	board = 0;
	key = 0;
	score = 0;
	natural_4s=0;
	add_random();
}


void end_ncurses(){
	endwin();
}

void end_game(){
	running = 0;
	mvprintw(22, 0, "Game Over");
	getch();
	end_ncurses();
	exit(0);
}
