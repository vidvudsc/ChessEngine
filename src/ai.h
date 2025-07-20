// ai.h - Minimal Negamax Alpha-Beta interface
#ifndef AI_H
#define AI_H

#include <stdbool.h>
#include "raylib.h"  // for Vector2 if needed externally

void AI_Init(int depth);            // set global search depth
void AI_PlayMove(bool sideIsWhite); // search and execute best move via ExecuteMove()
void AI_Quit(void);                 // (currently unused)

#endif
