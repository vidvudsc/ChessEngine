// pgn.c
#include "pgn.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// --- Externals we need from main / engine ---
extern char board[8][8];
extern int  halfMoveClock;
extern int  promotionFromX, promotionFromY, promotionToX, promotionToY;
extern int  chessBoardSize;
extern int  whiteKingMoved, blackKingMoved; // (not strictly required here)
extern int  whiteRookMoved[2], blackRookMoved[2];
extern int  selectedPieceX, selectedPieceY;
extern int  selectedMoveCount;
extern int  boardOffsetX, boardOffsetY;
extern int  screenWidth, screenHeight;
extern int  screenHeight;
extern int  screenWidth;
extern int  boardOffsetY;
extern int  boardOffsetX;
extern int  isDragging;
extern int  dragPieceX, dragPieceY;
extern int  dragPieceX, dragPieceY;
extern int  dragPieceY;
extern int  dragPieceX;
extern int  isWhiteTurn;      // **IMPORTANT**: AFTER ExecuteMove this has toggled
extern int  gameOver;

// Need functions to test legality/check state
extern int  IsKingInCheck(bool isWhiteKing);
extern int  IsValidMove(int sx,int sy,int ex,int ey);
extern Vector2 *GeneratePieceMoves(char piece,int sx,int sy,int *count);

// --------------------------------------------------

static PGNMove moves[PGN_MAX_MOVES];
static int moveCount = 0;
static char gameResult[8] = "*";

// Utility: square to algebraic file/rank
static inline void squareToAlg(int x,int y,char *out){
    out[0] = 'a' + x;
    out[1] = '8' - y;
    out[2] = 0;
}

const char* PGN_GetResult(void){ return gameResult; }

void PGN_SetResult(const char *res){
    strncpy(gameResult,res,7);
    gameResult[7]='\0';
}

// Determine if another same-type piece (same color) can also move to (endX,endY)
static void computeDisambiguation(char piece,int startX,int startY,int endX,int endY,
                                  char *needFile,char *needRank){
    *needFile = 0; *needRank = 0;
    if (tolower(piece)=='p' || tolower(piece)=='k') return; // pawns and kings only need disamb in rare underpromoted? (Pawns use file when capturing)
    // Scan board
    for(int y=0;y<8;y++){
        for(int x=0;x<8;x++){
            if (x==startX && y==startY) continue;
            if (board[y][x]==piece){
                if (IsValidMove(x,y,endX,endY)){
                    // conflict
                    if (startX != x) *needFile = 1;
                    if (startY != y) *needRank = 1;
                    // If same file and same rank can't happen (same square), but keep logic
                }
            }
        }
    }
    // Minimality rule: if both file and rank flagged but one alone distinguishes, prefer minimal
    if (*needFile && *needRank){
        // test if file alone enough
        int unique=1;
        for(int y=0;y<8;y++){
            for(int x=0;x<8;x++){
                if (x==startX && y==startY) continue;
                if (board[y][x]==piece){
                    if (x==startX){ // shares file
                        if (IsValidMove(x,y,endX,endY)){
                            unique=0; // file alone insufficient
                        }
                    }
                }
            }
        }
        if (unique) *needRank = 0;
        else {
            // test if rank alone enough
            unique=1;
            for(int y=0;y<8;y++){
                for(int x=0;x<8;x++){
                    if (x==startX && y==startY) continue;
                    if (board[y][x]==piece){
                        if (y==startY){
                            if (IsValidMove(x,y,endX,endY)){
                                unique=0;
                            }
                        }
                    }
                }
            }
            if (unique) *needFile = 0;
        }
    }
}

// Called *before* ExecuteMove mutates the board (pass capture info, etc.)
void PGN_RecordMoveBefore(int startX,int startY,int endX,int endY,char piece,char captured,
                          char promotion,bool wasCapture,bool wasCastle,bool wasCastleLong,bool wasEnPassant){
    if (moveCount >= PGN_MAX_MOVES) return;
    PGNMove *m = &moves[moveCount];
    m->startX = startX; m->startY = startY;
    m->endX = endX;     m->endY = endY;
    m->piece = piece;
    m->captured = captured;
    m->promotion = promotion;
    m->isWhite = isupper(piece);               // side moving *before* toggle
    m->moveNumber = m->isWhite ? (moveCount/2 + 1) : (moveCount/2 + 1);

    // Build base SAN (without +/# yet)
    char san[PGN_SAN_MAX]; san[0]=0;

    if (wasCastle){
        strcpy(san, wasCastleLong ? "O-O-O" : "O-O");
    } else {
        char fileNeeded=0, rankNeeded=0;
        char lower = tolower(piece);
        if (lower != 'p' && lower != 'k')
            computeDisambiguation(piece,startX,startY,endX,endY,&fileNeeded,&rankNeeded);

        int idx=0;
        if (lower != 'p'){
            // piece letter upper-case
            san[idx++] = toupper(lower);
            if (fileNeeded) san[idx++] = 'a'+startX;
            if (rankNeeded) san[idx++] = '8'-startY;
        } else {
            // pawn
            if (wasCapture){ // pawn capture needs source file
                san[idx++] = 'a'+startX;
            }
        }
        if (wasCapture){
            san[idx++] = 'x';
        }
        // Destination square
        san[idx++] = 'a'+endX;
        san[idx++] = '8'-endY;
        // Promotion (added before check symbol)
        if (promotion){
            san[idx++] = '=';
            san[idx++] = toupper(promotion);
        }
        san[idx] = 0;
    }
    strcpy(m->san,san);
    moveCount++;
}

// After board updated & turn toggled, determine + or #
void PGN_FinalizeLastMove(bool gaveCheck,bool gaveMate){
    if (moveCount==0) return;
    PGNMove *m = &moves[moveCount-1];
    if (gaveMate) {
        strncat(m->san,"#", PGN_SAN_MAX - strlen(m->san) - 1);
    } else if (gaveCheck) {
        strncat(m->san,"+", PGN_SAN_MAX - strlen(m->san) - 1);
    }
}

void PGN_Reset(void){
    moveCount = 0;
    strcpy(gameResult,"*");
}

static void printHeaders(void){
    // You can replace "Player" names or add more tags as needed
    printf("[Event \"Casual Game\"]\n");
    printf("[Site \"?\"]\n");
    printf("[Date \"????.??.??\"]\n");
    printf("[Round \"?\"]\n");
    printf("[White \"White\"]\n");
    printf("[Black \"Black\"]\n");
    printf("[Result \"%s\"]\n\n", gameResult);
}

void PGN_PrintGame(void){
    printHeaders();
    for(int i=0;i<moveCount;i++){
        if (moves[i].isWhite){
            // Start of a new full move
            printf("%d. %s", moves[i].moveNumber, moves[i].san);
        } else {
            // Black move: ensure spacing
            printf(" %s", moves[i].san);
        }
        // Add space after black move (or white if next is black)
        if (i+1 < moveCount){
            if (!moves[i].isWhite) printf(" ");
            else if (moves[i].isWhite && (i+1 < moveCount) && !moves[i+1].isWhite) printf(" ");
            else printf(" ");
        }
    }
    // Append result
    if (moveCount>0) printf(" %s\n", gameResult);
    else printf("%s\n", gameResult);
}

void PGN_PrintLastMove(void) {
    extern int moveCount;
    extern PGNMove moves[];
    if (moveCount > 0) {
        PGNMove *m = &moves[moveCount-1];
        if (m->isWhite)
            printf("%d. %s\n", m->moveNumber, m->san);
        else
            printf("%s\n", m->san);
    }
}
