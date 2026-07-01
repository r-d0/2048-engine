#pragma once
#include "game_state.h"

typedef enum{
	CAP_DEPTH,
	CAP_TIME,
	CAP_NODES,
}CapStyle;

extern CapStyle cap_style;
extern int MAX_DEPTH;
extern int SEARCH_MS;
extern int SEARCH_NODES;
extern int stop2048;

extern void precompute_rows();
extern void handle_movement();
