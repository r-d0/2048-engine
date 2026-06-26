#include "settings.h"

typedef enum{
	S_START,
	S_STOP2048,
	S_DEPTH,
	S_EXIT
}Selection;

static Selection selection = 0;
static int buttons = 4;

static int locked = 0;


void draw_settings(){
	mvprintw(0,0, "Settings");

	if (selection == S_START)
		attron(COLOR_PAIR(1));
	mvprintw(2,0, "Start game");
	attroff(COLOR_PAIR(1));

	if (selection == S_STOP2048)
		attron(COLOR_PAIR(1));
	mvprintw(3,0, "Diable engine at 2048: %s", stop2048 ? "Yes" : "No");
	attroff(COLOR_PAIR(1));
		
	if (selection == S_DEPTH)
		attron(COLOR_PAIR(1));
	mvprintw(4,0, "Engine Depth: %d%s", MAX_DEPTH, MAX_DEPTH > 7 ? "\t\tWARN: SLOW SPEEDS ABOVE DEPTH 7" : " ");
	attroff(COLOR_PAIR(1));

	if (selection == S_EXIT)
		attron(COLOR_PAIR(1));
	mvprintw(5,0, "Exit");
	attroff(COLOR_PAIR(1));
}


void settings_interaction(){
	if (key == 258){
		if (locked){
			if (selection == S_STOP2048){
				stop2048 = 0;
			}else if (selection == S_DEPTH){
				if (MAX_DEPTH > 1)
					MAX_DEPTH--;
			}
		}else{
			if (selection < buttons - 1)
				selection++;
		}
	} else if (key == 259){
		if (locked){
			if (selection == S_STOP2048){
				stop2048 = 1;
			}else if (selection == S_DEPTH){
				MAX_DEPTH++;
			}
		}else{
			if (selection > 0)
				selection--;
		}
	} else if (key == ' '){
		if (selection == S_START){
			start_round();
		}else if (selection == S_EXIT){
			end_game();
		}else if (selection == S_DEPTH || selection == S_STOP2048){
			locked = !locked;
		}
	}else if (key == 10){
		locked = 0;
		start_round();
	}

}

void enter_settings(){
	current_state = STATE_SETTINGS;
}
