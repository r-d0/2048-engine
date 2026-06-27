#include "movement.h"
#include <math.h>
#include <unistd.h>

int mask = 0xf;

struct timespec search_start;

static inline double elapsed_ms(struct timespec start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0 +
           (now.tv_nsec - start.tv_nsec) / 1e6;
}

int MAX_DEPTH = 10;
int SEARCH_MS = 100;
int CAP_ENGINE = 1;
int stop2048 = 0;

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
	return (u64)left_table[b  & 0xffff] | (u64)left_table[(b >> 16) & 0xffff] << 16 | (u64)left_table[(b >> 32) & 0xffff] << 32 | (u64)left_table[(b >> 48) & 0xffff] << 48; 
}

u64 move_right(u64 b) {
    return (u64)reverse_row(left_table[b & 0xffff]) | (u64)reverse_row(left_table[(b >> 16) & 0xffff]) << 16 | (u64)reverse_row(left_table[(b >> 32) & 0xffff]) << 32 | (u64)reverse_row(left_table[(b >> 48) & 0xffff]) << 48;
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


typedef struct{
	float prob;
	int val;
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


static inline float monotonicity(u64 b){
	// reward tiles decreasing consistently in one direction
	float result = 0;
	for (int row = 0; row < 4; row++){
		float inc = 0, dec = 0;
		float prev = (b >> (row << 4)) & 0xf;
		for (int col = 1; col < 4; col++){
			float cur = (b >> (((row << 2) | col) << 2)) & 0xf;
			float dif = prev - cur;
			dec += fmaxf(dif,0);
			inc += fmaxf(-dif, 0);
			prev = cur;
		}
		result -= fminf(inc,dec);
	}
	for (int col = 0; col < 4; col++){
		float inc = 0, dec = 0;
		float prev = (b >> (col << 2))  & 0xf;
		for (int row = 1; row < 4; row++){
			float cur = (b >> (((row << 2) | col) << 2)) & 0xf;
			float dif = prev - cur;
			dec += fmaxf(dif,0);
			inc += fmaxf(-dif, 0);
			prev = cur;
		}
		result -= fminf(inc, dec);
	}
	return result;
}

static inline float smoothness (u64 b){
	// Punish boards with large differences between side by side tiles
	float result = 0;
	for (int row = 0; row < 4; row++){
		float prev = (b >> (row << 4)) & 0xf;
		for (int col = 1; col < 4; col++){
			float cur = (b >> (((row << 2) | col) << 2)) & 0xf;
			float m = (prev > 0 && cur > 0) ? 1.0f : 0.0f;
			result -= fabsf(prev - cur) * m;
			prev = cur;
		}
	}
	for (int col = 0; col < 4; col++){
		float prev = (b >> (col << 2)) & 0xf;
		for (int row = 1; row < 4; row++){
			float cur = (b >> (((row << 2) | col) << 2)) & 0xf;
			float m = (prev > 0 && cur > 0) ? 1.0f : 0.0f;
			result -= fabsf(prev - cur) * m;
			prev = cur;
		}
	}
	return result;
}

float log2f_table[17] = {
	-10.0f, 0.0f, 1.0f, 1.5850f,
	2.0f, 2.3219f, 2.5850f, 2.8070f,
	3.0f, 3.1699, 3.3219, 3.4594,
	3.5850, 3.7004, 3.8074, 3.9069, 4.0f
};

static inline float log2board(u64 b){
	// reward sparce board, but only up to a point, log2 means that dense boards are punished more than empty boards are rewarded
	u16 empty = scan_empty(b);
	return log2f_table[__builtin_popcountll(empty)];
}

static inline float max_tile_corner(u64 b){
	int square = 0, max = 0;
	for (int i = 0; i < 16; i++){
		int val = (b >> (i<<2)) & 0xf;
		int m = -(val > max);
		max = (val & m) | (max & ~m);
		square = (i & m) | (square & ~m);

	}
	int corner = (0x1001000000001001 >> square) & 1;
	return (float)max * corner;
}

static inline int game_won (u64 b){
	for (int i = 0; i < 16; i++){
		if ((b & mask) == 0xb)
			return 1;
		b>>=4;
	}
	return 0;
}

static const float w_mono = 3.0f;
static const float w_smooth = 2.7f;
static const float w_empty = 2.7f;
static const float w_corner = 1.0f;

static float evaluate(u64 b){
	float result = 0;
	result += w_mono*monotonicity(b);
	result += w_smooth*smoothness(b);
	result += w_empty*log2board(b);
	result += w_corner*max_tile_corner(b);
	return result;
}


static u64 placetile(u64 b, int cell, int val){
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

static int timed_out = 0;

static int node_count=0;
#define TIME_CHECK_INTERVAL 1

static float p_cell_table[17] = {
	0.0f, 1.0f, 0.5f, 0.3333f, 0.25f, 0.2f, 0.16667f, 0.142857f, 0.125f, 0.1111f, 0.1f, 0.090909f, 0.083333f, 0.076923f, 0.071429f, 0.066667f, 0.0625f
};


float engine(u64 b, int depth, int is_player){
	if (CAP_ENGINE && !(node_count++ & (TIME_CHECK_INTERVAL-1))) {
		if (elapsed_ms(search_start) > SEARCH_MS) {
			timed_out = 1;
			return 0;
		}
	}
	if (depth == 0)
		return evaluate(b);
	if (is_player){
		float best = -(float)(0u - 1);
		int any_move = 0;
		for (int move = 0; move < 4; move++){
			u64 newboard = connect_move(b, move);
			if (newboard == b)
				continue;
			any_move = 1;
			float score = engine(newboard, depth - 1, 0);
			best = fmaxf(best,score);
		}
		if (!any_move) return evaluate(b);
		return best;
	}else{
		u16 empty = scan_empty(b);
		if (!empty) return evaluate(b);
		float total = 0;
		float p_cell = p_cell_table[__builtin_popcountll(empty)];
		float p09= p_cell * 0.9f;
		float p01= p_cell * 0.1f;
		for (int cell = 0; cell < 16; cell++){
			if (!((empty >> cell) & 1))
				continue;
			u64 nb1 = placetile(b, cell, 1);
			total += p09 * engine(nb1, depth-1, 1);
			u64 nb2 = placetile(b, cell, 2);
			total += p01 * engine(nb2, depth-1, 1);
		}
		return total;
	}
}


static int best_move(u64 b, int depth){
	int best = 0;
	depth_reached = depth;
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


int best_move_timed(u64 b){
	clock_gettime(CLOCK_MONOTONIC, &search_start);
	int best = 0;
	for (int depth = 1; depth < MAX_DEPTH; depth++){
		timed_out = 0;
		node_count = 0;
		int result = best_move(b, depth);
		if (timed_out) break;
		best = result;
		depth_reached = depth;
	}
	return best;
}

void try_auto(){
	clock_gettime(CLOCK_MONOTONIC, &start_frame);
	int best;
	if (CAP_ENGINE)
		best= best_move_timed(board);
	else
		best = best_move(board, MAX_DEPTH);
	board = connect_move(board, best);
	score = get_game_score();
	add_random();
	clock_gettime(CLOCK_MONOTONIC, &end);
	usleep(10000);
}

void handle_movement(){
	if ((unsigned int)key - 258 < 4){
		u64 newboard = connect_move(board,key - 258);
		if (board == newboard)
			return;
		board = newboard;

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
		if ((stop2048 && game_won(board)) || game_over(board))
			auto_on = 0;
		else
			try_auto();
	}

}

