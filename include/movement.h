#pragma once
#include "game_state.h"

extern int MAX_DEPTH;
extern int SEARCH_MS;
extern int CAP_ENGINE;
extern int stop2048;

extern void precompute_rows();
extern void handle_movement();
