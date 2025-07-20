// ai.c – smarter, less deterministic, depth‑adaptive search
// -----------------------------------------------------------------------------
//  New features requested:
//  • Black searches one ply shallower than White automatically.
//  • Random tie‑break among the top moves (±15 cp window) – avoids ‘always the
//    same game’ while staying objectively strong.
//  • Depth adapts to material count: we keep full depth in the opening, but
//    once ≤14 pieces remain we drop one ply, keeping late‑game latency low.
//
//  No changes are needed anywhere else – AI_Init(depth) is still the only
//  function you call.  Internally we set:
//      whiteDepth = depth,  blackDepth = depth‑1 (min 1)
//
//  Compile as before.
// -----------------------------------------------------------------------------

#include "ai.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "pgn.h"
#include <stdint.h>

// --- External engine hooks ---------------------------------------------------
extern char  board[8][8];
extern bool  isWhiteTurn;
extern void  ExecuteMove(char piece,int sx,int sy,int ex,int ey,char promo);
extern bool  IsValidMove(int sx,int sy,int ex,int ey);
extern Vector2 *GeneratePieceMoves(char piece,int sx,int sy,int *count);
extern bool  IsKingInCheck(bool isWhite);

// -----------------------------------------------------------------------------
static int DEPTH_WHITE = 5;
static int DEPTH_BLACK = 5; // white‑1 by default
static const int INF = 100000000;

// --- Piece values ------------------------------------------------------------
static const int VAL[128] = { ['p']=100,['n']=320,['b']=330,['r']=500,['q']=900,['k']=0 };
static inline int valueOf(char p){ return VAL[(int)tolower(p)]; }

// --- Tiny piece‑square tables -------------------------------------------------
static const int PST_P[64]={ 0, 5, 5,-10,-10, 5, 5, 0, 0,10,-5, 0, 0,-5,10, 0, 0,10,10,20,20,10,10, 0, 5,20,20,30,30,20,20, 5,10,25,25,35,35,25,25,10,50,50,50,60,60,50,50,50,70,70,70,70,70,70,70,70, 0, 0, 0, 0, 0, 0, 0, 0};
static const int PST_N[64]={-50,-40,-30,-30,-30,-30,-40,-50,-40,-20,-5,-5,-5,-5,-20,-40,-30,-5,10,15,15,10,-5,-30,-30,-5,15,20,20,15,-5,-30,-30,-5,15,20,20,15,-5,-30,-30,-5,10,15,15,10,-5,-30,-40,-20,-5,-5,-5,-5,-20,-40,-50,-40,-30,-30,-30,-30,-40,-50};
static const int PST_B[64]={-20,-10,-10,-10,-10,-10,-10,-20,-10, 5, 0, 0, 0, 0, 5,-10,-10,10,10,10,10,10,10,-10,-10, 0,10,10,10,10, 0,-10,-10, 5, 5,10,10, 5, 5,-10,-10, 0, 5,10,10, 5, 0,-10,-10, 0, 0, 0, 0, 0, 0,-10,-20,-10,-10,-10,-10,-10,-10,-20};
static const int PST_R[64]={ 0, 0, 5,10,10, 5, 0, 0,-5, 0, 0, 0, 0, 0, 0,-5,-5, 0, 0, 0, 0, 0, 0,-5,-5, 0, 0, 0, 0, 0, 0,-5,-5, 0, 0, 0, 0, 0, 0,-5,-5, 0, 0, 0, 0, 0, 0,-5, 5,10,10,10,10,10,10, 5, 0, 0, 0, 0, 0, 0, 0};
static const int PST_Q[64]={-20,-10,-10,-5,-5,-10,-10,-20,-10, 0, 0, 0, 0, 0, 0,-10,-10, 0, 5, 5, 5, 5, 0,-10,-5, 0, 5, 5, 5, 5, 0,-5, 0, 0, 5, 5, 5, 5, 0,-5,-10, 5, 5, 5, 5, 5, 0,-10,-10, 0, 5, 0, 0, 0, 0,-10,-20,-10,-10,-5,-5,-10,-10,-20};
static const int PST_K[64]={-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-30,-40,-40,-50,-50,-40,-40,-30,-20,-30,-30,-40,-40,-30,-30,-20,-10,-20,-20,-20,-20,-20,-20,-10, 20,20, 0, 0, 0, 0,20,20, 20,30,10, 0, 0,10,30,20};
static inline const int* pstFor(char p){ switch(tolower(p)){case 'p':return PST_P;case 'n':return PST_N;case 'b':return PST_B;case 'r':return PST_R;case 'q':return PST_Q;case 'k':return PST_K;} return PST_P; }
static inline int pstIdx(int x,int y,bool white){ return white? y*8+x : 63-(y*8+x);} // mirror for black

// --- Eval --------------------------------------------------------------------
static int evaluate(){
    int score=0; int pieces=0;
    for(int y=0;y<8;y++)for(int x=0;x<8;x++){char pc=board[y][x];if(pc==' ')continue;pieces++;int t=valueOf(pc)+pstFor(pc)[pstIdx(x,y,isupper(pc))];score+=isupper(pc)?t:-t;}
    return score;
}

// --- Move (max 256) ----------------------------------------------------------
typedef struct{int sx,sy,ex,ey;char pc,cap;bool promo;int order;}Move;
static void push(Move*m){m->pc=board[m->sy][m->sx];m->cap=board[m->ey][m->ex];m->promo=false;char mv=m->pc;if(tolower(m->pc)=='p'&&((isupper(m->pc)&&m->ey==0)||(!isupper(m->pc)&&m->ey==7))){mv=isupper(m->pc)?'Q':'q';m->promo=true;}board[m->sy][m->sx]=' ';board[m->ey][m->ex]=mv;isWhiteTurn=!isWhiteTurn;}
static void pop(Move*m){isWhiteTurn=!isWhiteTurn;board[m->sy][m->sx]=m->pc;board[m->ey][m->ex]=m->cap;}

// --- Generate + basic MVV‑LVA ordering --------------------------------------
static int genMoves(Move*out){int n=0;bool side=isWhiteTurn;for(int y=0;y<8;y++)for(int x=0;x<8;x++){char pc=board[y][x];if(pc==' '||side!=isupper(pc))continue;int mc=0;Vector2*mv=GeneratePieceMoves(pc,x,y,&mc);for(int i=0;i<mc;i++){int ex=(int)mv[i].x,ey=(int)mv[i].y;if(!IsValidMove(x,y,ex,ey))continue;char cap=board[ey][ex];int mvv=valueOf(cap),lva=valueOf(pc);int score=(cap!=' '?10000+mvv-lva:0);out[n++] = (Move){x,y,ex,ey,0,0,false,score};}free(mv);} // insertion sort
    for(int i=1;i<n;i++){Move k=out[i];int j=i-1;while(j>=0&&out[j].order<k.order){out[j+1]=out[j];j--;}out[j+1]=k;}return n;}

// --- Zobrist hashing and transposition table ---------------------------------
typedef struct { uint64_t key; int depth; int score; int flag; int best; } TT;
#define TT_SIZE (1<<20) // 1M entries
static TT tt[TT_SIZE];
static uint64_t zobrist[12][64], zobristWhite, zobristCastle[16], zobristEnPassant[8];
static uint64_t hashKey = 0;

static void initZobrist() {
    for (int p = 0; p < 12; ++p)
        for (int sq = 0; sq < 64; ++sq)
            zobrist[p][sq] = ((uint64_t)rand() << 32) | rand();
    zobristWhite = ((uint64_t)rand() << 32) | rand();
    for (int i = 0; i < 16; ++i) zobristCastle[i] = ((uint64_t)rand() << 32) | rand();
    for (int i = 0; i < 8; ++i) zobristEnPassant[i] = ((uint64_t)rand() << 32) | rand();
}

static int pieceToZIdx(char pc) {
    switch (pc) {
        case 'P': return 0; case 'N': return 1; case 'B': return 2; case 'R': return 3; case 'Q': return 4; case 'K': return 5;
        case 'p': return 6; case 'n': return 7; case 'b': return 8; case 'r': return 9; case 'q': return 10; case 'k': return 11;
    }
    return -1;
}

// Helper to get castling rights as a 4-bit value: KQkq
static int getCastlingRights() {
    int rights = 0;
    // White king/rook unmoved
    extern bool whiteKingMoved, blackKingMoved;
    extern bool whiteRookMoved[2], blackRookMoved[2];
    if (!whiteKingMoved && !whiteRookMoved[1]) rights |= 1; // K
    if (!whiteKingMoved && !whiteRookMoved[0]) rights |= 2; // Q
    if (!blackKingMoved && !blackRookMoved[1]) rights |= 4; // k
    if (!blackKingMoved && !blackRookMoved[0]) rights |= 8; // q
    return rights;
}
// Helper to get en passant file index (0-7) or -1 if none
static int getEnPassantFile() {
    extern Vector2 enPassantCaptureSquare;
    if (enPassantCaptureSquare.y == -1 || enPassantCaptureSquare.x < 0 || enPassantCaptureSquare.x > 7) return -1;
    return (int)enPassantCaptureSquare.x;
}

static uint64_t hashBoard() {
    uint64_t h = 0;
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        int idx = pieceToZIdx(board[y][x]);
        if (idx >= 0) h ^= zobrist[idx][y*8+x];
    }
    if (!isWhiteTurn) h ^= zobristWhite;
    // Add castling rights
    int cr = getCastlingRights();
    h ^= zobristCastle[cr];
    // Add en passant file
    int ep = getEnPassantFile();
    if (ep >= 0) h ^= zobristEnPassant[ep];
    return h;
}

static inline int ttProbe(int depth, int alpha, int beta, int *best) {
    TT *e = &tt[hashKey & (TT_SIZE-1)];
    if (e->key == hashKey) {
        if (e->depth >= depth) {
            if (e->flag == 0) return e->score; // exact
            if (e->flag == 1 && e->score <= alpha) return alpha;
            if (e->flag == 2 && e->score >= beta ) return beta;
        }
        *best = e->best;
    }
    return INF+1;
}
static inline void ttStore(int depth, int score, int flag, int best) {
    TT *e = &tt[hashKey & (TT_SIZE-1)];
    if (depth >= e->depth) {
        e->key = hashKey;
        e->depth = depth;
        e->score = score;
        e->flag = flag;
        e->best = best;
    }
}

// --- Alpha‑beta --------------------------------------------------------------
static int alphaBeta(int depth,int alpha,int beta){
    if(depth==0)return evaluate();
    Move mv[256];
    int n=genMoves(mv);
    if(n==0)return IsKingInCheck(isWhiteTurn)?-INF+depth:0;
    int bestMove=-1, flag=1, bestScore=-INF;
    hashKey = hashBoard();
    int ttScore = ttProbe(depth,alpha,beta,&bestMove);
    if(ttScore!=INF+1) return ttScore;
    // If bestMove from TT exists, swap it to front
    if(bestMove>=0 && bestMove<n){
        Move tmp=mv[0]; mv[0]=mv[bestMove]; mv[bestMove]=tmp;
    }
    for(int i=0;i<n;i++){
        push(&mv[i]);
        int s=-alphaBeta(depth-1,-beta,-alpha);
        pop(&mv[i]);
        if(s>bestScore){ bestScore=s; bestMove=i; }
        if(s>alpha){ alpha=s; flag=0; }
        if(alpha>=beta){ flag=2; break; }
    }
    ttStore(depth,bestScore,flag,bestMove);
    return bestScore;
}

// --- Adaptive depth for current side ----------------------------------------
static int sideDepth(void){int pieces=0;for(int y=0;y<8;y++)for(int x=0;x<8;x++)if(board[y][x]!=' ')pieces++;int base=isWhiteTurn?DEPTH_WHITE:DEPTH_BLACK; if(pieces<=14&&base>1)base--; return base;}

// --- Public API --------------------------------------------------------------
void AI_Init(int depth){ if(depth<1)depth=1; DEPTH_WHITE=depth; DEPTH_BLACK=(depth>1?depth-1:1); srand((unsigned)time(NULL)); initZobrist(); }
void AI_Quit(void){}

void AI_PlayMove(bool /*sideIsWhite*/){ Move mv[256];int n=genMoves(mv);if(n==0)return;int bestScore=-INF;int cand[256];int candN=0;int d=sideDepth();for(int i=0;i<n;i++){push(&mv[i]);int s=-alphaBeta(d-1,-INF,INF);pop(&mv[i]);if(s>bestScore){bestScore=s;candN=0;cand[candN++]=i;}else if(s>=bestScore-15){ // within 15 cp → candidate
            cand[candN++]=i;} }
    int choice=cand[rand()%candN];Move m=mv[choice];char p=board[m.sy][m.sx];char promo=0;if(tolower(p)=='p'&&((isupper(p)&&m.ey==0)||(!isupper(p)&&m.ey==7)))promo=isupper(p)?'Q':'q';ExecuteMove(p,m.sx,m.sy,m.ex,m.ey,promo);PGN_PrintLastMove(); }
