#include "settings.h"
#include "math.h"

typedef enum{
	S_START,
	S_SEED,
	S_STOP2048,
	S_CAPSTYLE,
	S_CAPLIMIT,
	S_EXIT
}Selection;

static Selection selection = 0;
static const int buttons = 6;

static int typing = 0;

void draw_settings(){
	// SETTINGS
	//
	// START GAME
	// DISABLE AT 2048?
	// STYLE: STATIC DEPTH, TIME LIMIT, NODE LIMIT
	// DEPTH / TIME LIMIT / NODE LIMIT
	// EXIT
	mvprintw(0,0, "Settings");

	int b = 2;
	if (selection == S_START)
		attron(COLOR_PAIR(1));
	mvprintw(b++,0, "Start game");
	attroff(COLOR_PAIR(1));

	if (selection == S_SEED)
		attron(COLOR_PAIR(1));
	mvprintw(b++,0, "Seed: %d",seed);
	attroff(COLOR_PAIR(1));

	if (selection == S_STOP2048)
		attron(COLOR_PAIR(1));
	mvprintw(b++,0, "Diable engine at 2048: %s", stop2048 ? "Yes" : "No");
	attroff(COLOR_PAIR(1));
		
	char capstyle[32];
	char captype[32];
	int capvalue = 0;
	switch (cap_style){
		case CAP_DEPTH:
			sprintf(capstyle,"Static Depth");
			sprintf(captype, "Depth");
			capvalue = MAX_DEPTH;
			break;
		case CAP_TIME:
			sprintf(capstyle,"Timed");
			sprintf(captype,"Max Time");
			capvalue = SEARCH_MS;
			break;
		case CAP_NODES:
			sprintf(capstyle,"Nodes");
			sprintf(captype,"Max Nodes");
			capvalue = SEARCH_NODES;
			break;
	}
	if (selection == S_CAPSTYLE)
		attron(COLOR_PAIR(1));
	mvprintw(b++,0, "Engine Cap Type: %s", capstyle);
	attroff(COLOR_PAIR(1));

	if (selection == S_CAPLIMIT)
		attron(COLOR_PAIR(1));
	mvprintw(b++,0, "%s: %d", captype, capvalue);
	attroff(COLOR_PAIR(1));

	if (selection == S_EXIT)
		attron(COLOR_PAIR(1));
	mvprintw(b++,0, "Exit");
	attroff(COLOR_PAIR(1));
}

void settings_interaction(){
	if (typing){
		if (key == 10){
			typing = 0;
			return;
		}else if (key == KEY_BACKSPACE){
			seed /= 10;
		}
		int num = key - '0';
		if (num >= 0 && num < 10){
			seed *= 10;
			seed += num;
		}
		return;
	}

	if (key == 258){
		if (selection < buttons - 1)
			selection++;
	} else if (key == 259){
		if (selection > 0)
			selection--;
	} else if (key == ' '){
		if (selection == S_START){
			start_round();
		}else if (selection == S_EXIT){
			end_game();
		}else if (selection == S_SEED){
			typing = !typing;
		}
	}else if (key == 10){
		start_round();
	}else if (key == 260){
		if (selection == S_STOP2048){
			stop2048 = 0;
		}else if (selection == S_CAPSTYLE){
			if (cap_style > 0)
				cap_style--;
		}else if (selection == S_CAPLIMIT){
			int* limit;
			switch(cap_style){
				case CAP_DEPTH:
					limit = &MAX_DEPTH;
					break;
				case CAP_TIME:
					limit = &SEARCH_MS;
					break;
				case CAP_NODES:
					limit = &SEARCH_NODES;
					break;
				default:
					return;
			}
			if (!limit)
				return;
			if (*limit == 1)
				return;
			if (*limit <= 50){
				*limit-=1;
			}else{
				int magnitude = 0;
				int firstdigit = *limit;
				int firsttwo = *limit;
				while (firstdigit >= 10){
					magnitude++;
					firsttwo = firstdigit;
					firstdigit /= 10;
				}
				if (firsttwo == 10){
					*limit -= 0.5*pow(10,magnitude-1);
				} else if (firsttwo <= 20){
					*limit -= pow(10,magnitude-1);
				} else if (firsttwo<= 40){
					*limit -= 2*pow(10,magnitude-1);
				}else{
					*limit -= 5*pow(10,magnitude-1);
				}

			}
		}
	}else if (key == 261){
		if (selection == S_STOP2048){
			stop2048 = 1;
		}else if (selection == S_CAPSTYLE){
			if (cap_style < 2)
				cap_style++;
		}else if (selection == S_CAPLIMIT){
			int* limit;
			switch(cap_style){
				case CAP_DEPTH:
					limit = &MAX_DEPTH;
					break;
				case CAP_TIME:
					limit = &SEARCH_MS;
					break;
				case CAP_NODES:
					limit = &SEARCH_NODES;
					break;
				default:
					return;
			}
			if (!limit)
				return;
			if (*limit < 50){
				*limit +=1;
			}else{
				int magnitude = 0;
				int firstdigit = *limit;
				int firsttwo = *limit;
				while (firstdigit >= 10){
					magnitude++;
					firsttwo = firstdigit;
					firstdigit /= 10;
				}
				if (firsttwo >= 40){
					*limit += 5*pow(10,magnitude-1);
				} else if (firsttwo >= 20){
					*limit += 2*pow(10,magnitude-1);
				}else{
					*limit += pow(10,magnitude-1);
				}

			}
		}
	}

}

void enter_settings(){
	current_state = STATE_SETTINGS;
}
