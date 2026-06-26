#include "game_state.h"
#include "movement.h"
#include "menu.h"
#include "settings.h"
#include "board.h"


void handle_menu(){
	erase();
	menu_interaction();
	draw_menu();
	refresh();
}

void handle_playing(){
	erase();
	if (key == 'r')
		start_round();
	handle_movement();
	draw_board();
	refresh();
}

void handle_settings(){
	erase();
	settings_interaction();
	draw_settings();
	refresh();
}

void tick(){
	if (current_state == STATE_MENU)
		handle_menu();
	if (current_state == STATE_SETTINGS)
		handle_settings();
	if (current_state == STATE_PLAYING)
		handle_playing();
	key = getch();
	if (key == 'q')
		enter_menu();
}

int main(){
	init_ncurses();
	precompute_rows();
	init_state();
	enter_menu();
	while (running){
		tick();
	}
	end_ncurses();
	return 0;
}
