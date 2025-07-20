#include "ai.h"
#include <stdlib.h>
#include <ctype.h>
#include "pgn.h"

extern char board[8][8];
extern int isWhiteTurn;
extern void ExecuteMove(char piece, int sx, int sy, int ex, int ey, char promo);
extern void CheckKingsCount(void);
extern void PGN_PrintLastMove(void);
extern void srand(unsigned int seed);
extern int rand(void);
extern void *malloc(unsigned long size);
extern void free(void *ptr);
extern int abs(int x);
extern int whiteKingMoved, blackKingMoved;
extern int whiteRookMoved[2], blackRookMoved[2];
extern int promotionPending;
extern int promotionFromX, promotionFromY, promotionToX, promotionToY;
extern int promotionColor;
extern int promotionChosen;
extern int enPassantCaptureSquare;
extern int lastStart;
extern int lastEnd;
extern int boardOffsetX;
extern int boardOffsetY;
extern int chessBoardSize;
extern int selectedPieceX, selectedPieceY;
extern int selectedMoves;
extern int selectedMoveCount;
extern int dragPieceX, dragPieceY;
extern int isDragging;
extern int gameOver;

// Forward declarations from main.c
extern int IsValidMove(int sx, int sy, int ex, int ey);
extern void *GeneratePieceMoves(char piece, int sx, int sy, int *moveCount);

void AI_Init(int depth) { (void)depth; }
void AI_Quit(void) { }

void AI_PlayMove(bool sideIsWhite) {
    typedef struct { int startX, startY, endX, endY; } Move;
    Move *moves = NULL;
    int n = 0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            char piece = board[y][x];
            if ((sideIsWhite && isupper(piece)) || (!sideIsWhite && islower(piece))) {
                int moveCount = 0;
                void *pm = GeneratePieceMoves(piece, x, y, &moveCount);
                if (moveCount > 0) {
                    Vector2 *vec = (Vector2 *)pm;
                    for (int i = 0; i < moveCount; i++) {
                        int ex = (int)vec[i].x, ey = (int)vec[i].y;
                        if (IsValidMove(x, y, ex, ey)) {
                            moves = realloc(moves, (n + 1) * sizeof(Move));
                            moves[n++] = (Move){x, y, ex, ey};
                        }
                    }
                }
                free(pm);
            }
        }
    }
    if (n > 0) {
        int idx = rand() % n;
        char p = board[moves[idx].startY][moves[idx].startX];
        char promo = 0;
        if (tolower(p) == 'p' && ((isupper(p) && moves[idx].endY == 0) || (!isupper(p) && moves[idx].endY == 7)))
            promo = isupper(p) ? 'Q' : 'q';
        ExecuteMove(p, moves[idx].startX, moves[idx].startY, moves[idx].endX, moves[idx].endY, promo);
        PGN_PrintLastMove();
    }
    free(moves);
    CheckKingsCount();
}
