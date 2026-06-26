#include "menu.h"
#include "settings.h"

typedef enum{
	S_START,
	S_EXIT
}Selection;

static Selection selection = 0;
static int buttons = 2;



void draw_menu(){
	mvprintw(0,0, "Ruadhan's 2048");

	if (selection == S_START)
		attron(COLOR_PAIR(1));
	mvprintw(2,0, "Start game");
	attroff(COLOR_PAIR(1));

	if (selection == S_EXIT)
		attron(COLOR_PAIR(1));
	mvprintw(3,0, "Exit");
	attroff(COLOR_PAIR(1));
}

void menu_interaction(){
	if (key == 258){
		if (selection < buttons - 1)
			selection++;
	} else if (key == 259){
		if (selection > 0)
			selection--;
	} else if (key == ' '){
		if (selection == S_START){
			key = 0;
			enter_settings();
		}else if (selection == S_EXIT){
			end_game();
		}
	}

}

void enter_menu(){
	erase();
	draw_menu();
	refresh();
	current_state = STATE_MENU;
}
