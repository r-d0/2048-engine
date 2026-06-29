#include "movement.h"
#include <math.h>
#include <unistd.h>

int mask = 0xf;

struct timespec search_start;

#define TT_SIZE 1048576 // 1 << 20
#define TT_MASK 1048575 // TT_SIZE - 1

typedef struct{
	u64 key;
	u64 val;
}TTEntry;

TTEntry tt[TT_SIZE];

static inline double elapsed_ms(struct timespec start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - start.tv_sec) * 1000.0 +
           (now.tv_nsec - start.tv_nsec) / 1e6;
}



static inline void tt_store(u64 board, float score, u8 depth){
	u32 score_bits;
	memcpy(&score_bits, &score, 4);
	u64 val_bits = ((u64)depth << 32) | score_bits;
	TTEntry* e = &tt[board & TT_MASK];
	e->val = val_bits;
	e->key = board ^ val_bits;
}


static inline int tt_lookup(u64 board, u8 depth, float* out){
	TTEntry *e = &tt[board & TT_MASK];
	u64 val_bits = e->val;

	if ((e->key ^ val_bits) != board) return 0;
	if (depth > (val_bits >> 32)) return 0;
	u32 score_bits = (u32)(val_bits & 0xffffffff);
	float out_val;
	memcpy(&out_val, &score_bits, 4);
	*out = out_val;
	return 1;

}

int MAX_DEPTH = 10;
int SEARCH_MS = 100;
int CAP_ENGINE = 1;
int stop2048 = 0;


u16 left_table[65536];
float heur_table[65536];

#define MONO_POWER   4.0f
#define MONO_WEIGHT  47.0f
#define SUM_POWER    3.5f
#define SUM_WEIGHT   11.0f
#define MERGE_WEIGHT 700.0f
#define EMPTY_WEIGHT 270.0f
#define LOST_PENALTY 200000.0f

void precompute_rows() {
    for (int row = 0; row < 65536; row++) {
        int t[4] = {
            row & 0xf,
            (row >> 4) & 0xf,
            (row >> 8) & 0xf,
            (row >> 12) & 0xf,
        };

        // --- move table ---
        u8 out[4] = {0};
        int free = 0;
        int merged[4] = {0};
        for (int i = 0; i < 4; i++) {
            if (!t[i]) continue;
            if (free && out[free-1] == t[i] && !merged[free-1]) {
                out[free-1]++;
                merged[free-1] = 1;
            } else {
                out[free++] = t[i];
            }
        }
        left_table[row] = out[0] | out[1]<<4 | out[2]<<8 | out[3]<<12;

        // --- heuristic table ---
        float sum = 0;
        int empty = 0, merges = 0;
        int prev = 0, counter = 0;

        for (int i = 0; i < 4; i++) {
            int rank = t[i];
            sum += powf(rank, SUM_POWER);
            if (rank == 0) {
                empty++;
            } else {
                if (prev == rank) {
                    counter++;
                } else if (counter > 0) {
                    merges += 1 + counter;
                    counter = 0;
                }
                prev = rank;
            }
        }
        if (counter > 0)
            merges += 1 + counter;

        float mono_left = 0, mono_right = 0;
        for (int i = 1; i < 4; i++) {
            if (t[i-1] > t[i])
                mono_left  += powf(t[i-1], MONO_POWER) - powf(t[i], MONO_POWER);
            else
                mono_right += powf(t[i],   MONO_POWER) - powf(t[i-1], MONO_POWER);
        }

        heur_table[row] = LOST_PENALTY
            + EMPTY_WEIGHT * empty
            + MERGE_WEIGHT * merges
            - MONO_WEIGHT  * fminf(mono_left, mono_right)
            - SUM_WEIGHT   * sum;
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
    return (u64)reverse_row(left_table[reverse_row(b & 0xffff)]) | (u64)reverse_row(left_table[reverse_row((b >> 16) & 0xffff)]) << 16 | (u64)reverse_row(left_table[reverse_row((b >> 32) & 0xffff)]) << 32 | (u64)reverse_row(left_table[reverse_row((b >> 48) & 0xffff)]) << 48;
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

static inline int game_won (u64 b){
	for (int i = 0; i < 16; i++){
		if ((b & mask) == 0xb)
			return 1;
		b>>=4;
	}
	return 0;
}


static inline float evaluate(u64 b) {
    u64 t = transpose(b);
    return heur_table[(b >> 0)  & 0xffff]
         + heur_table[(b >> 16) & 0xffff]
         + heur_table[(b >> 32) & 0xffff]
         + heur_table[(b >> 48) & 0xffff]
         + heur_table[(t >> 0)  & 0xffff]
         + heur_table[(t >> 16) & 0xffff]
         + heur_table[(t >> 32) & 0xffff]
         + heur_table[(t >> 48) & 0xffff];
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
#define CPROB_THRESH 0.000001f

static float p_cell_table[17] = {
	0.0f, 1.0f, 0.5f, 0.3333f, 0.25f, 0.2f, 0.16667f, 0.142857f, 0.125f, 0.1111f, 0.1f, 0.090909f, 0.083333f, 0.076923f, 0.071429f, 0.066667f, 0.0625f
};


float engine(u64 b, int depth, int is_player, float cprob){
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
			float score = engine(newboard, depth - 1, 0, cprob);
			best = fmaxf(best,score);
		}
		if (!any_move) return evaluate(b);
		return best;
	}else{
		u16 empty = scan_empty(b);
		if (!empty) return evaluate(b);
		float ttval;
		int hit = tt_lookup(b, depth, &ttval);
		if (hit)
			return ttval;

		float total = 0;
		float p_cell = p_cell_table[__builtin_popcountll(empty)];
		float p09= p_cell * 0.9f;
		float p01= p_cell * 0.1f;
		for (int cell = 0; cell < 16; cell++){
			if (!((empty >> cell) & 1))
				continue;
			u64 nb1 = placetile(b, cell, 1);
			if (p09 * cprob > CPROB_THRESH)
				total += p09 * engine(nb1, depth-1, 1, cprob * p09);
			u64 nb2 = placetile(b, cell, 2);
			if (p01 * cprob > CPROB_THRESH)
				total += p01 * engine(nb2, depth-1, 1, cprob * p01);
		}
		tt_store(b, total, depth);
		return total;
	}
}


static int best_move(u64 b, int depth){
	timed_out = 0;
	node_count = 0;
	int best = 0;
	depth_reached = depth;
	float best_score = -(float)(0u-1);
	for (int move = 0; move < 4; move++){
		u64 newboard = connect_move(b, move);
		if (newboard == b)
			continue;
		float score = engine(newboard, depth-1, 0, 1.0f);
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

