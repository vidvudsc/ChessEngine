// pgn.h
#ifndef PGN_H
#define PGN_H
#include "raylib.h"
#include <stdbool.h>

#define PGN_MAX_MOVES 512
#define PGN_SAN_MAX   16   // enough for long SAN like Nbdxe5+ etc.

typedef struct {
    int  moveNumber;          // full-move number (1-based)
    bool isWhite;             // true if white's move
    int  startX, startY;
    int  endX, endY;
    char piece;               // piece moved (original, before promotion)
    char captured;            // captured piece (if any)
    char promotion;           // promotion piece (Q,R,B,N in correct color)
    char san[PGN_SAN_MAX];    // SAN string
} PGNMove;

void PGN_Reset(void);
void PGN_RecordMoveBefore(int startX,int startY,int endX,int endY,char piece,char captured,char promotion,
                          bool wasCapture,bool wasCastle,bool wasCastleLong,bool wasEnPassant);
void PGN_FinalizeLastMove(bool gaveCheck,bool gaveMate);
void PGN_SetResult(const char *res);   // "1-0","0-1","1/2-1/2","*"
void PGN_PrintGame(void);              // prints full PGN to stdout
void PGN_PrintLastMove(void);
const char* PGN_GetResult(void);

#endif
