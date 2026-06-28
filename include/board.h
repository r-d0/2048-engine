#pragma once
#include <stdint.h>
#include <ncurses.h>
#include <time.h>

#define u64 uint64_t
#define u32 uint32_t
#define u16 uint16_t
#define u8 uint8_t

extern struct timespec start, start_frame, end;
extern int depth_reached;

extern u64 board;
extern int score;
extern int natural_4s;
extern void draw_board();
extern u16 scan_empty(u64 b);
extern void add_random();
