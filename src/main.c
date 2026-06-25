#include "game_state.h"
#include "movement.h"
#include "board.h"


void tick(){
	erase();
	handle_movement();
	draw_board();
	refresh();
	key = getch();
	if (key == 'q')
		running = 0;
	if (key == 'r')
		init_game();
}

int main(){
	init_ncurses();
	init_game();
	while (running){
		tick();
	}
	end_ncurses();
	return 0;
}
