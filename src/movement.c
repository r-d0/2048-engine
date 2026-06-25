#include "movement.h"
#include <math.h>
#include <unistd.h>

char mask = 0xf;
int auto_on = 0;

int DEPTH = 5;
//#define to2048 

typedef struct{
	u64* b;
	char square, pos, tile, free_cell, merge_val, move;
}MergeInfo;

static void slide(MergeInfo* m){
	// swad = 0123
	*m->b &= ~((u64)mask << m->pos);
	char dest = 0;
	switch(m->move){
		case 0:
			dest = m->square + (m->free_cell << 2);
			break;
		case 1:
			dest = m->square - (m->free_cell << 2);
			break;
		case 2:
			dest = m->square - m->free_cell;
			break;
		case 3:
			dest = m->square + m->free_cell;
			break;
		default:
			return;
	}
	*m->b |= (u64)m->tile << (dest << 2);

}

static int merge(MergeInfo* m){
	if (!(m->merge_val && m->merge_val == m->tile))
		return 0;
	char dest = 0;
	switch(m->move){
		case 0:
			dest = m->square + ((m->free_cell + 1) << 2);
			break;
		case 1:
			dest = m->square - ((m->free_cell + 1) << 2);
			break;
		case 2:
			dest = m->square - (m->free_cell + 1);
			break;
		case 3:
			dest = m->square + (m->free_cell + 1);
			break;
		default:
			return 0;
	}
	*m->b &= ~(((u64)mask << m->pos) | (u64)mask << (dest<<2));
	*m->b |= (u64)++m->tile << (dest << 2);
	m->free_cell++;
	return m->tile;
}


static u64 make_move(u64 b, int move){
	MergeInfo m;
	m.b = &b;
	m.move = move;
	switch(move){
		case 0:
			for (int col = 0; col < 4; col++){
				m.free_cell = 0;
				m.merge_val = 0;
				for (int row = 3; row >= 0; row--){
					m.square = row << 2 | col;
					m.pos = m.square << 2;
					m.tile = (*m.b >> m.pos) & mask;
					if (m.tile){
						int increase = merge(&m);
						if (!increase && m.free_cell){
							slide(&m);
						}
						m.merge_val = m.tile;
					}else{
						m.free_cell++;
					}
				}
			}
			break;
		case 1:
			for (int col = 0; col < 4; col++){
				m.free_cell = 0;
				m.merge_val = 0;
				for (int row = 0; row < 4; row++){
					m.square = row << 2 | col;
					m.pos = m.square << 2;
					m.tile = (*m.b >> m.pos) & mask;
					if (m.tile){
						int increase = merge(&m);
						if (!increase && m.free_cell){
							slide(&m);
						}
						m.merge_val = m.tile;
					}else{
						m.free_cell++;
					}
				}
			}
			break;
		case 2:
			for (int row = 0; row < 4; row++){
				m.free_cell = 0;
				m.merge_val = 0;
				for (int col = 0; col < 4; col++){
					m.square = row << 2 | col;
					m.pos = m.square << 2;
					m.tile = (*m.b >> m.pos) & mask;
					if (m.tile){
						int increase = merge(&m);
						if (!increase && m.free_cell){
							slide(&m);
						}
						m.merge_val = m.tile;
					}else{
						m.free_cell++;
					}
				}
			}
			break;
		case 3:
			for (int row = 0; row < 4; row++){
				m.free_cell = 0;
				m.merge_val = 0;
				for (int col = 3; col >= 0; col--){
					m.square = row << 2 | col;
					m.pos = m.square << 2;
					m.tile = (*m.b >> m.pos) & mask;
					if (m.tile){
						int increase = merge(&m);
						if (!increase && m.free_cell){
							slide(&m);
						}
						m.merge_val = m.tile;
					}else{
						m.free_cell++;
					}
				}
			}
			break;
	}
	return b;
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
			u64 newboard = make_move(b, move);
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
		u64 newboard = make_move(b, move);
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
	start_frame = clock();
	int best = best_move(board,DEPTH);
	board = make_move(board, best);
	score = get_game_score();
	add_random();
	usleep(10000);
	end = clock();
}
void handle_movement(){
	if ((unsigned int)key - 258 < 4){
		board = make_move(board,key - 258);
		score = get_game_score();
		add_random();
	}
	if (key == 'x' || key == 'c'){
		auto_on = !auto_on;
		nodelay(stdscr, auto_on);
		if (auto_on)
			start = clock();
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

