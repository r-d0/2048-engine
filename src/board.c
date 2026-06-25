#include "board.h"
#include <stdlib.h>

u64 board;
int natural_4s = 0;
int score;
clock_t start;
clock_t start_frame;
clock_t end;

void draw_board(){
	char x, y;
	x = y = 1;
	char mask = 0xfull;
	for (char row = 0; row < 4; row++){
		for (char col = 0; col < 4; col++){
			move(y,x);
			struct{
				char square : 4;
				char value : 4;
			} s;
			s.square = (row << 2) | col; 
			s.value = (board >> (s.square << 2)) & mask;
			if (!s.value)
				printw(".");
			else
				printw("%d", 1<<s.value);

			x += 6;
		}
		x = 1;
		y += 3;
	}
	mvprintw(0,0,"Score: %d",score);
	mvprintw(24,0,"Time: %0.6lf, dt: %0.6lf",(double)(end - start)/CLOCKS_PER_SEC, (double)(end - start_frame)/CLOCKS_PER_SEC);
}

u16 scan_empty(u64 b){
	u16 empty = 0;
	for (int i = 0; i < 16; i++){
		if (!(b & (0xfull << (i << 2))))
			empty |= 1ull << i;
	}
	return empty;
}

void add_random(){
	u16 empty = scan_empty(board);
	u16 total = __builtin_popcountll(empty);
	if (!total) return;
	int ran = rand();
	total = ran % total;
	while (total--){
		empty &= empty-1;
	}
	char pos = __builtin_ctzll(empty) << 2;
	u64 val;
	if (ran % 10){
		val = 1ull;
	}else{
		val = 2ull;
		natural_4s++;

	}
	board |= val << pos;
	
}


