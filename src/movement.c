#include "movement.h"
#include <math.h>
#include <unistd.h>

char mask = 0xf;
int auto_on = 0;

int DEPTH = 7;
 #define to2048 

u16 left_table[65536];

void precompute_rows(){
	for (int  row = 0; row < 65536; row++){
		u8 tiles[] = {
			row & 0xf,
			(row >> 4) & 0xf,
			(row >> 8) & 0xf,
			(row >> 12) & 0xf,
		};
		u8 out[4] = {0};
		int free = 0;
		int merged[4] = {0};
		for (int i = 0; i < 4; i++){
			if (!tiles[i]) continue;
			if (free && out[free-1] == tiles[i] && !merged[free-1]){
				out[free-1]++;
				merged[free-1] = 1;
			}else
				out[free++] = tiles[i];
		}
		left_table[row] = out[0] | out[1] << 4 | out[2] << 8 | out[3] << 12;
	}
}

u16 reverse_row(u16 row){
	return (row >> 12) | ((row >> 4) & 0xf0) | ((row << 4) & 0xf00) | (row << 12);
}

u64 transpose(u64 b) {
    u64 t = b;
    t = (t & 0xf0f00f0ff0f00f0fULL)
      | ((t & 0x0000f0f00000f0f0ULL) << 12)
      | ((t & 0x0f0f00000f0f0000ULL) >> 12);
    t = (t & 0xff00ff0000ff00ffULL)
      | ((t & 0x00000000ff00ff00ULL) << 24)
      | ((t & 0x00ff00ff00000000ULL) >> 24);
    return t;
}



u64 move_left(u64 b){
	u64 result = 0;
	for (int row = 0; row < 4; row++){
		result |= (u64)left_table[(b >> (row * 16)) & 0xffff] << (row * 16);
	}
	return result;
}

u64 move_right(u64 b) {
    u64 result = 0;
    for (int row = 0; row < 4; row++) {
        u16 rev = reverse_row((b >> (row*16)) & 0xffff);
        result |= (u64)reverse_row(left_table[rev]) << (row*16);
    }
    return result;
}

u64 move_up(u64 b) {
    u64 t = transpose(b);
    return transpose(move_left(t));
}

u64 move_down(u64 b) {
    u64 t = transpose(b);
    return transpose(move_right(t));
}

u64 connect_move(u64 b, int move){
	switch(move){
		case 0: return  move_down(b);
		case 1: return  move_up(b);
		case 2: return  move_left(b);
		case 3: return  move_right(b);
		default: return b;
	}
}

//TODO: game_over, evaluate, trymove, placetile
//TODO: check if empty causes definition issues within engine - conflict with game_state

typedef struct{
	float prob;
	char val;
}TileProb;

TileProb chances[2] = {
	{0.9f, 1},
	{0.1f, 2}
};


static int game_over(u64 b){

	// empty cells
	u64 a = b;
	a |= a >> 1;
	a |= a >> 2;
	if (~a & 0x1111111111111111ULL)
		return 0;

	// horizontal merges
	u64 h = b;
	h ^= (h>>4);
	h |= 0xf000f000f000f000ull;

	h |= h >> 1;
	h |= h >> 2;
	if (~h & 0x1111111111111111ull)
		return 0;

	// vertical merges
	u64 v = b;
	v ^= (v >> 16);
	v |= 0xffff000000000000ULL;

	v |= v >> 1;
	v |= v >> 2;
	if (~v & 0x1111111111111111ULL)
		return 0;
	return 1;
}

//TODO: monotonicity, smoothness, log2, max_tile_corner
//TODO: ADD math.h and link against it

static float monotonicity(u64 b){
	// reward tiles decreasing consistently in one direction
	float result = 0;
	for (char row = 0; row < 4; row++){
		float inc = 0, dec = 0;
		for (char col = 0; col < 3; col++){
			float square_val = (b >> (((row << 2) | col) << 2)) & 0xf;
			float dest_val = (b >> (((row << 2) | (col+1)) << 2)) & 0xf;
			if (square_val > dest_val) dec += square_val - dest_val;
			if (square_val < dest_val) inc += dest_val - square_val;
		}
		result -= (inc < dec) ? inc : dec;
	}
	for (char col = 0; col < 4; col++){
		float inc = 0, dec = 0;
		for (char row = 0; row < 3; row++){
			float square_val = (b >> (((row << 2) | col) << 2)) & 0xf;
			float dest_val = (b >> ((((row+1) << 2) | col) << 2)) & 0xf;
			if (square_val > dest_val) dec += square_val - dest_val;
			if (square_val < dest_val) inc += dest_val - square_val;
		}
		result -= (inc < dec) ? inc : dec;
	}
	return result;
}

static float smoothness (u64 b){
	// Punish boards with large differences between side by side tiles
	float result = 0;
	for (char row = 0; row < 4; row++){
		for (char col = 0; col < 3; col++){
			float square_val = (b >> (((row << 2) | col) << 2)) & 0xf;
			float dest_val = (b >> ((((row + 1) << 2) | col) << 2)) & 0xf;
			if (square_val && dest_val)
				result -= fabsf(square_val - dest_val);
		}
	}
	for (char col = 0; col < 4; col++){
		for (char row = 0; row < 3; row++){
			float square_val = (b >> (((row << 2) | col) << 2)) & 0xf;
			float dest_val = (b >> (((row << 2) | (col+1)) << 2)) & 0xf;
			if (square_val && dest_val)
				result -= fabsf(square_val - dest_val);
		}
	}
	return result;
}

static float log2board(u64 b){
	// reward sparce board, but only up to a point, log2 means that dense boards are punished more than empty boards are rewarded
	float result;
	u16 empty = scan_empty(b);
	result = empty ? log2f((float)__builtin_popcountll(empty)) : -10.0f; // could go wrong
	return result;
}

static float max_tile_corner(u64 b){
	char square = 1, max = 0;
	for (int i = 0; i < 16; i++){
		char val = (b >> (i<<2)) & 0xf;
		if (val > max){
			max = val;
			square = i;
		}
	}
	if (square == 0 || square == 3 || square == 12 || square == 15)
		return (float)max;
	return 0.0f;
}

#ifdef to2048
static int game_won (u64 b){
	for (int i = 0; i < 16; i++){
		if ((b & mask) == 0xb)
			return 1;
		b>>=4;
	}
	return 0;
}
#endif

static float evaluate(u64 b){
	float result = 0;
	result += 3.0f*monotonicity(b);
	result += 2.7f*smoothness(b);
	result += 2.7f*log2board(b);
	result += 1.0f*max_tile_corner(b);
//	result += (float)(0u-1)*game_won(b);
	return result;
}


static u64 placetile(u64 b, int cell, char val){
	int pos = cell << 2;
	return (b | (u64)val << pos); 
}

static int get_game_score(){
	int calc = 0 - (natural_4s << 2);
	for (int i = 0; i < 16; i++){
		int val = (board >> (i << 2)) & mask;
		if (!val)
			continue;
		calc += (1 << val) * (val - 1); 
	}
	return calc;
}


float engine(u64 b, int depth, int is_player){
	if (depth == 0 || game_over(b))
		return evaluate(b);
	if (is_player){
		float best = -(float)(0u - 1);
		for (int move = 0; move < 4; move++){
			u64 newboard = connect_move(b, move);
			if (newboard == b)
				continue;
			float score = engine(newboard, depth - 1, 0);
			best = (best > score) ? best : score;
		}
		return best;
	}else{
		u16 empty = scan_empty(b);
		if (!empty) return evaluate(b);
		float total = 0;
		float p_cell = 1.0 / __builtin_popcountll(empty);
		for (int cell = 0; cell < 16; cell++){
			if (!((empty >> cell) & 1))
				continue;
			for (int i = 0; i < 2; i++){
				float prob = chances[i].prob;
				char val = chances[i].val;
				u64 newboard = placetile(b, cell, val);
				float score = engine(newboard, depth - 1, 1);
				total += p_cell * prob * score;

			}
		}
		return total;

	}
}

static int best_move(u64 b, int depth){
	int best = 0;
	float best_score = -(float)(0u-1);
	for (int move = 0; move < 4; move++){
		u64 newboard = connect_move(b, move);
		if (newboard == b)
			continue;
		float score = engine(newboard, depth-1, 0);
		if (score > best_score){
			best_score = score;
			best = move;
		}
	}
	return best;
}


void try_auto(){
	clock_gettime(CLOCK_MONOTONIC, &start_frame);
	int best = best_move(board,DEPTH);
	board = connect_move(board, best);
	score = get_game_score();
	add_random();
	clock_gettime(CLOCK_MONOTONIC, &end);
	usleep(10000);
}
void handle_movement(){
	if ((unsigned int)key - 258 < 4){
		board = connect_move(board,key - 258);
		score = get_game_score();
		add_random();
	}
	if (key == 'x' || key == 'c'){
		auto_on = !auto_on;
		nodelay(stdscr, auto_on);
		if (auto_on)
			clock_gettime(CLOCK_MONOTONIC, &start);
	}
	if (auto_on){
#ifdef to2048
#define cond game_over(board) || game_won(board)
#else
#define cond game_over(board)
#endif
		if (cond)
			auto_on = 0;
		else
			try_auto();
	}

}

