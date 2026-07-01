#include "board.h"
#include <stdlib.h>

u64 board;
int natural_4s = 0;
int score;
int tt_miss_nodes;
int depth_reached;
int seed;
struct timespec start, end, start_frame;

#define TDIFF(a, b) ((b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9)


void draw_board(){
	int x, y;
	x = y = 1;
	int mask = 0xfull;
	for (int row = 0; row < 4; row++){
		for (int col = 0; col < 4; col++){
			move(y,x);
			int square, value;
			square = (row << 2) | col; 
			value = (board >> (square << 2)) & mask;
			if (!value)
				printw(".");
			else
				printw("%d", 1<<value);

			x += 6;
		}
		x = 1;
		y += 3;
	}
	mvprintw(0,0,"Score: %d",score);
	mvprintw(24,0,"Time: %0.6lf, dt: %0.6lf, depth: %d, node_count: %d",TDIFF(start, end), TDIFF(start_frame, end), depth_reached,tt_miss_nodes);
	mvprintw(25,0,"Seed: %d",seed);
}

u16 scan_empty(u64 b){
	u64 a = b | (b >> 1) | (b >> 2) | (b >> 3);
	a = ~a & 0x1111111111111111ULL;
	u16 result = 0;
	for (int i = 0; i < 16; i++){
		result |= ((a >> (i*4)) & 1) << i;
	}
	return result;
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


