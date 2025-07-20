#define RAYGUI_IMPLEMENTATION
#include "raylib.h"
#include "raygui.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ai.h"
#include "pgn.h"
#include <stdbool.h>

// --- PGN move tracking globals ---
#define MAX_MOVES 300
// Remove PGNMove struct, pgnMoves, pgnMoveCount, PrintMove, PrintFullPGN, MoveToSAN, CopyBoard, and all PGN/move printing logic.
// Remove any PGN-related button/key logic and references in ExecuteMove and elsewhere.

// Remove the duplicate PGNMove typedef from main.c, and only use the definition from ai.h.
// typedef struct {
//     int startX, startY, endX, endY;
//     char piece; // moved piece
//     char captured; // captured piece (if any)
//     char promotion; // promotion piece (if any)
//     int moveNumber; // 1-based
//     int isWhite; // 1 if white, 0 if black
// } PGNMove;
// PGNMove pgnMoves[MAX_MOVES];
// int pgnMoveCount = 0;

// --- Add global Sound variables ---
Sound moveSound; // For normal moves
Sound captureSound; // For captures

 // Game mode related variables
typedef enum {
        MODE_NORMAL,
        MODE_PLAY_WHITE,
        MODE_PLAY_BLACK,
        MODE_AI_VS_AI
} GameMode;

GameMode currentMode = MODE_NORMAL;

char board[8][8];
Vector2 dragOffset;
Texture2D chessPieces;
bool isDragging = false;
Vector2 positions[8][8];
int dragPieceX = -1, dragPieceY = -1;
Vector2 enPassantCaptureSquare = {-1, -1}; 
Vector2 lastStart = {-1, -1}, lastEnd = {-1, -1};
int boardOffsetX = 0;
int boardOffsetY = 0;
const int screenWidth = 1200;
const int screenHeight = 800;
int chessBoardSize = 0;

// --- Persistent selection state for move highlighting ---
int selectedPieceX = -1, selectedPieceY = -1;
Vector2 *selectedMoves = NULL;
int selectedMoveCount = 0;

const Color DGRAY = {35, 38, 46, 255}; 
const Color WHITE_SQUARE = {215, 192, 162, 255};
const Color BLACK_SQUARE = {131, 105, 83, 255};
const Color START_POS_COLOR = {255, 214, 10, 128};  
const Color END_POS_COLOR = {255, 195, 0, 128};  

int halfMoveClock = 0; // To track the number of half-moves (each player's turn is a half-move)
bool isStalemate = false;
bool isWhiteTurn = true; // White starts
bool whiteKingMoved = false, blackKingMoved = false;
bool whiteRookMoved[2] = {false, false}, blackRookMoved[2] = {false, false};

// --- Game over state (move these above CheckKingsCount) ---
bool gameOver = false;
char* gameOverMessage = "";

bool IsKingInCheck(bool isWhiteKing);
bool IsOpponentPiece(char piece, int x, int y);
bool IsValidMove(int startX, int startY, int endX, int endY);
void ExecuteMove(char piece, int startX, int startY, int endX, int endY, char promotionPiece);
Vector2 *GeneratePieceMoves(char piece, int startX, int startY, int *moveCount);
void UpdateCastlingRights(char piece, int startX, int startY, int endX, int endY);

void LoadChessPieces() {
    Image image = LoadImage("images/chesspiecesL.png");
    if (image.data == NULL) {
        printf("Failed to load images/chesspiecesL.png, trying ../images/chesspiecesL.png\n");
        image = LoadImage("../images/chesspiecesL.png");
        if (image.data == NULL) {
            printf("Failed to load chesspiecesL.png from both paths\n");
            return; // Don't try to load a texture from a NULL image
        } else {
            printf("Loaded ../images/chesspiecesL.png successfully!\n");
        }
    } else {
        printf("Loaded images/chesspiecesL.png successfully!\n");
    }
    chessPieces = LoadTextureFromImage(image);
    UnloadImage(image);
}
void DrawChessBoard() {
    Color checkColor = {255, 0, 0, 128}; // Red color for check
    bool whiteKingInCheck = IsKingInCheck(true);
    bool blackKingInCheck = IsKingInCheck(false);

    int squareSize = chessBoardSize / 8;  // Use chessBoardSize to calculate squareSize
    int borderWidth = 15; // Adjust the thickness of the border
    Color borderColor = {82, 81, 85, 255}; // The border color
    Color cornerColor = DARKGRAY; // Dark gray color for corners

    // Draw the border with corners
    DrawRectangle(boardOffsetX - borderWidth, boardOffsetY - borderWidth, squareSize * 8 + 2 * borderWidth, squareSize * 8 + 2 * borderWidth, borderColor);
    DrawRectangleLinesEx((Rectangle){boardOffsetX - borderWidth, boardOffsetY - borderWidth, squareSize * 8 + 2 * borderWidth, squareSize * 8 + 2 * borderWidth}, borderWidth, borderColor);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            Color squareColor = (x + y) % 2 == 0 ? WHITE_SQUARE : BLACK_SQUARE;

            Rectangle rect = {boardOffsetX + x * squareSize, boardOffsetY + y * squareSize, squareSize, squareSize};
            DrawRectangleRec(rect, squareColor);

            // Highlight last move
            if (lastStart.x == x && lastStart.y == y) {
                DrawRectangleRec(rect, START_POS_COLOR);
            }
            if (lastEnd.x == x && lastEnd.y == y) {
                DrawRectangleRec(rect, END_POS_COLOR);
            }
            // Highlight king's position in check
            if ((whiteKingInCheck && board[y][x] == 'K') ||
                (blackKingInCheck && board[y][x] == 'k')) {
                DrawRectangleRec(rect, checkColor);
            }
            // Highlight selected piece with yellow
            if (selectedPieceX == x && selectedPieceY == y) {
                Color yellow = {255, 255, 0, 120};
                DrawRectangleRec(rect, yellow);
                // Optionally, draw a border for extra clarity:
                DrawRectangleLinesEx(rect, 3, (Color){255, 215, 0, 255});
            }
        }
    }

    // Show legal moves for the piece being dragged, or for the selected piece
    if (isDragging) {
        int moveCount = 0;
        Vector2 *moves = GeneratePieceMoves(board[dragPieceY][dragPieceX], dragPieceX, dragPieceY, &moveCount);
        for (int i = 0; i < moveCount; i++) {
            int moveX = (int)moves[i].x;
            int moveY = (int)moves[i].y;
            if (IsValidMove(dragPieceX, dragPieceY, moveX, moveY)) {
                Rectangle moveRect = {boardOffsetX + moveX * squareSize, boardOffsetY + moveY * squareSize, squareSize, squareSize};
                Color lightBlue = {173, 216, 230, 100};
                DrawRectangleRec(moveRect, lightBlue);
            }
        }
        free(moves);
    } else if (selectedPieceX != -1 && selectedPieceY != -1 && selectedMoves != NULL) {
        for (int i = 0; i < selectedMoveCount; i++) {
            int moveX = (int)selectedMoves[i].x;
            int moveY = (int)selectedMoves[i].y;
            if (IsValidMove(selectedPieceX, selectedPieceY, moveX, moveY)) {
                Rectangle moveRect = {boardOffsetX + moveX * squareSize, boardOffsetY + moveY * squareSize, squareSize, squareSize};
                Color lightBlue = {173, 216, 230, 100};
                DrawRectangleRec(moveRect, lightBlue);
            }
        }
    }

    // Draw the fully covered corners
    DrawRectangle(boardOffsetX - borderWidth, boardOffsetY - borderWidth, borderWidth, borderWidth, cornerColor);
    DrawRectangle(boardOffsetX + 8 * squareSize, boardOffsetY - borderWidth, borderWidth, borderWidth, cornerColor);
    DrawRectangle(boardOffsetX - borderWidth, boardOffsetY + 8 * squareSize, borderWidth, borderWidth, cornerColor);
    DrawRectangle(boardOffsetX + 8 * squareSize, boardOffsetY + 8 * squareSize, borderWidth, borderWidth, cornerColor);
}

void DrawPiece(char piece, int x, int y, bool isDragged) {
    int squareSize = chessBoardSize / 8;
    // Updated for new sprite size: 2880x960, each piece is 480x480
    Rectangle sourceRect = { 0.0f, 0.0f, 480.0f, 480.0f };
    int row = (piece >= 'a' && piece <= 'z') ? 1 : 0;
    int col;
    switch (tolower(piece)) {
        case 'k': col = 0; break;
        case 'q': col = 1; break;
        case 'b': col = 2; break;
        case 'n': col = 3; break;
        case 'r': col = 4; break;
        case 'p': col = 5; break;
        default:  return;
    }
    sourceRect.x = col * 480.0f;
    sourceRect.y = row * 480.0f;

    // Adjust position to be relative to the centered chessboard
    Vector2 position = { boardOffsetX + x * squareSize, boardOffsetY + y * squareSize };
    if (!isDragged) {
        positions[y][x] = position;
    }

    Rectangle destRect = { position.x, position.y, squareSize, squareSize };
    if (isDragged) {
        Vector2 mousePos = GetMousePosition();
        destRect.x = mousePos.x - dragOffset.x;
        destRect.y = mousePos.y - dragOffset.y;
    }

    DrawTexturePro(chessPieces, sourceRect, destRect, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void DrawPieces() {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (board[y][x] != ' ' && !(isDragging && x == dragPieceX && y == dragPieceY)) {
                DrawPiece(board[y][x], x, y, false);
            }
        }
    }
    if (isDragging) {
        DrawPiece(board[dragPieceY][dragPieceX], dragPieceX, dragPieceY, true);
    }
}

// --- AI move structure for full move info ---
typedef struct {
    int startX, startY, endX, endY;
} Move;

// --- FIXED SetupBoardFromFEN with bounds and FEN field check ---
void SetupBoardFromFEN(const char *fen) {
    memset(board, ' ', sizeof(board));
    int x = 0, y = 0;
    const char *ptr = fen;
    // Piece placement
    while (*ptr && *ptr != ' ') {
        char c = *ptr++;
        if (c == '/') { y++; x = 0; continue; }
        if (isdigit((unsigned char)c)) {
            int run = c - '0';
            while (run-- && x < 8) board[y][x++] = ' ';
        } else {
            if (x < 8 && y < 8) board[y][x++] = c;
        }
        if (y >= 8) break;
    }
    // Side to move
    while (*ptr == ' ') ptr++;
    isWhiteTurn = (*ptr == 'w');
    while (*ptr && *ptr != ' ') ptr++;
    // Castling rights
    while (*ptr == ' ') ptr++;
    whiteKingMoved = blackKingMoved = true;
    whiteRookMoved[0] = whiteRookMoved[1] = true;
    blackRookMoved[0] = blackRookMoved[1] = true;
    if (*ptr != '-') {
        for (; *ptr && *ptr != ' '; ptr++) {
            switch (*ptr) {
                case 'K': whiteKingMoved = false; whiteRookMoved[1] = false; break;
                case 'Q': whiteKingMoved = false; whiteRookMoved[0] = false; break;
                case 'k': blackKingMoved = false; blackRookMoved[1] = false; break;
                case 'q': blackKingMoved = false; blackRookMoved[0] = false; break;
            }
        }
    } else {
        ptr++;
    }
    // En passant
    while (*ptr == ' ') ptr++;
    if (*ptr != '-') {
        int file = ptr[0] - 'a';
        int rank = 8 - (ptr[1] - '0');
        enPassantCaptureSquare.x = file;
        enPassantCaptureSquare.y = rank;
        ptr += 2;
    } else {
        enPassantCaptureSquare.x = -1;
        enPassantCaptureSquare.y = -1;
        ptr++;
    }
    // (Optionally: parse halfmove/fullmove clocks here if needed)
}

// --- PrintBoard for debugging ---
void PrintBoard() {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++)
            putchar(board[r][c] == ' ' ? '.' : board[r][c]);
        putchar('\n');
    }
}

// --- CheckKingsCount returns bool and aborts on error ---
bool CheckKingsCount() {
    int whiteKings = 0, blackKings = 0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (board[y][x] == 'K') whiteKings++;
            else if (board[y][x] == 'k') blackKings++;
        }
    }
    if (whiteKings != 1 || blackKings != 1) {
        printf("Error: Invalid king count detected!\n");
        gameOver = true;
        gameOverMessage = "Invalid king count!";
        return false;
    }
    return true;
}


bool IsKingInCheck(bool isWhiteKing) {
    // Find the king's position
    char king = isWhiteKing ? 'K' : 'k';
    int kingX = -1, kingY = -1;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (board[y][x] == king) {
                kingX = x;
                kingY = y;
                break;
            }
        }
        if (kingX != -1) break;
    }

    // Check if any opponent's piece can attack the king's position
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (board[y][x] != ' ' && IsOpponentPiece(king, x, y)) {
                int moveCount;
                Vector2 *moves = GeneratePieceMoves(board[y][x], x, y, &moveCount);
                for (int i = 0; i < moveCount; i++) {
                    if (moves[i].x == kingX && moves[i].y == kingY) {
                        free(moves);
                        return true;
                    }
                }
                free(moves);
            }
        }
    }
    return false;
}

bool IsPathClear(int startX, int startY, int endX, int endY) {
    int dx = (endX - startX) != 0 ? (endX - startX) / abs(endX - startX) : 0;
    int dy = (endY - startY) != 0 ? (endY - startY) / abs(endY - startY) : 0;

    int x = startX + dx;
    int y = startY + dy;

    while (x != endX || y != endY) {
        if (board[y][x] != ' ') return false;
        x += dx;
        y += dy;
    }

    return true;
}

// --- FIXED IsOpponentPiece with bounds check ---
bool IsOpponentPiece(char piece, int x, int y) {
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return false;
    if (board[y][x] == ' ') return false;
    return isupper(piece) != isupper(board[y][x]);
}


Vector2 *GeneratePieceMoves(char piece, int startX, int startY, int *moveCount) {
    Vector2 *moves = NULL;
    *moveCount = 0;

    // Helper macro to add a move
    #define ADD_MOVE(x, y) \
        if ((x) >= 0 && (x) < 8 && (y) >= 0 && (y) < 8) { \
            if (board[(y)][(x)] == ' ' || IsOpponentPiece(piece, (x), (y))) { \
                moves = realloc(moves, ((*moveCount) + 1) * sizeof(Vector2)); \
                moves[*moveCount] = (Vector2){(x), (y)}; \
                (*moveCount)++; \
            } \
        }

    // Helper macro to add line moves (for rook, bishop, queen)
    #define ADD_LINE_MOVES(dx, dy) \
        { \
            int x = startX + (dx); \
            int y = startY + (dy); \
            while (x >= 0 && x < 8 && y >= 0 && y < 8) { \
                if (board[y][x] != ' ' && !IsOpponentPiece(piece, x, y)) break; \
                ADD_MOVE(x, y); \
                if (board[y][x] != ' ') break; \
                x += (dx); \
                y += (dy); \
            } \
        }

    switch (tolower(piece)) {
        case 'p': { // Pawn
            int direction = isupper(piece) ? -1 : 1;
            // Forward move
            if (board[startY + direction][startX] == ' ') {
                ADD_MOVE(startX, startY + direction);
                // Initial two-square move
                if ((isupper(piece) && startY == 6) || (!isupper(piece) && startY == 1)) {
                    if (board[startY + 2 * direction][startX] == ' ') {
                        ADD_MOVE(startX, startY + 2 * direction);
                    }
                }
            }
            // Captures
            if (IsOpponentPiece(piece, startX - 1, startY + direction)) {
                ADD_MOVE(startX - 1, startY + direction);
            }
            if (IsOpponentPiece(piece, startX + 1, startY + direction)) {
                ADD_MOVE(startX + 1, startY + direction);
            }
            // En passant capture
            if (startY == (isupper(piece) ? 3 : 4)) {
                if (enPassantCaptureSquare.x == startX - 1 && enPassantCaptureSquare.y == startY + direction) {
                    ADD_MOVE(startX - 1, startY + direction);
                }
                if (enPassantCaptureSquare.x == startX + 1 && enPassantCaptureSquare.y == startY + direction) {
                    ADD_MOVE(startX + 1, startY + direction);
                }
            }
            break;
        }
        case 'n': { // Knight
            int knightMoves[8][2] = {{-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};
            for (int i = 0; i < 8; i++) {
                int x = startX + knightMoves[i][0];
                int y = startY + knightMoves[i][1];
                if ((x) >= 0 && (x) < 8 && (y) >= 0 && (y) < 8) {
                    if (board[y][x] == ' ' || IsOpponentPiece(piece, x, y)) {
                        ADD_MOVE(x, y);
                    }
                }
            }
            break;
        }
        case 'r': { // Rook
            ADD_LINE_MOVES(1, 0);
            ADD_LINE_MOVES(-1, 0);
            ADD_LINE_MOVES(0, 1);
            ADD_LINE_MOVES(0, -1);
            break;
        }
        case 'b': { // Bishop
            ADD_LINE_MOVES(1, 1);
            ADD_LINE_MOVES(-1, 1);
            ADD_LINE_MOVES(1, -1);
            ADD_LINE_MOVES(-1, -1);
            break;
        }
        case 'q': { // Queen
            ADD_LINE_MOVES(1, 0);
            ADD_LINE_MOVES(-1, 0);
            ADD_LINE_MOVES(0, 1);
            ADD_LINE_MOVES(0, -1);
            ADD_LINE_MOVES(1, 1);
            ADD_LINE_MOVES(-1, 1);
            ADD_LINE_MOVES(1, -1);
            ADD_LINE_MOVES(-1, -1);
            break;
        }
        case 'k': { // King
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx != 0 || dy != 0) ADD_MOVE(startX + dx, startY + dy);
                }
            }
            // Castling logic
            if (isupper(piece)) { // White king
                if (!whiteKingMoved) {
                    // Castling kingside (short castling)
                    if (!whiteRookMoved[1] && board[startY][5] == ' ' && board[startY][6] == ' ') {
                        ADD_MOVE(6, startY);
                    }
                    // Castling queenside (long castling)
                    if (!whiteRookMoved[0] && board[startY][1] == ' ' && board[startY][2] == ' ' && board[startY][3] == ' ') {
                        ADD_MOVE(2, startY);
                    }
                }
            } else { // Black king
                if (!blackKingMoved) {
                    // Castling kingside (short castling)
                    if (!blackRookMoved[1] && board[startY][5] == ' ' && board[startY][6] == ' ') {
                        ADD_MOVE(6, startY);
                    }
                    // Castling queenside (long castling)
                    if (!blackRookMoved[0] && board[startY][1] == ' ' && board[startY][2] == ' ' && board[startY][3] == ' ') {
                        ADD_MOVE(2, startY);
                    }
                }
            }
            break;
        }
    }

    #undef ADD_MOVE
    #undef ADD_LINE_MOVES
    return moves;
}


Vector2 *GenerateMoves(bool isWhite, int *totalMoveCount) {
    Vector2 *allMoves = NULL;
    *totalMoveCount = 0;
    int moveCount;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if ((isWhite && isupper(board[y][x])) || (!isWhite && islower(board[y][x]))) {
                Vector2 *pieceMoves = GeneratePieceMoves(board[y][x], x, y, &moveCount);
                if (moveCount > 0) {
                    allMoves = realloc(allMoves, (*totalMoveCount + moveCount) * sizeof(Vector2));
                    memcpy(allMoves + *totalMoveCount, pieceMoves, moveCount * sizeof(Vector2));
                    *totalMoveCount += moveCount;
                    free(pieceMoves);
                }
            }
        }
    }
    return allMoves;
}

bool IsValidMove(int startX, int startY, int endX, int endY) {
    // Validate the basic movement rules
    int moveCount;
    Vector2 *moves = GeneratePieceMoves(board[startY][startX], startX, startY, &moveCount);

    bool validBasicMove = false;
    for (int i = 0; i < moveCount; i++) {
        if (moves[i].x == endX && moves[i].y == endY) {
            validBasicMove = true;
            break;
        }
    }
    free(moves);

    if (!validBasicMove) {
        return false; // The move is not valid according to basic chess rules
    }

    char tempStart = board[startY][startX];
    char tempEnd = board[endY][endX];
    bool isWhiteMove = isupper(tempStart);

    // --- Castling path check: clear destination square if occupied by opponent ---
    if (tolower(tempStart) == 'k' && abs(startX - endX) == 2) {
        int castleY = isWhiteMove ? 7 : 0;
        int direction = (endX > startX) ? 1 : -1;
        bool previouslyMoved = isWhiteMove ? whiteKingMoved : blackKingMoved; // store original state
        char savedSquares[3] = {' ', ' ', ' '};
        int idx = 0;
        for (int x = startX + direction; x != endX + direction; x += direction) {
            char orig = board[castleY][x];
            if ((orig != ' ' && x != endX) || IsKingInCheck(isWhiteMove)) {
                return false; // The king is moving through or into check
            }
            // Temporarily move the king for check detection
            board[castleY][startX] = ' ';
            if (x == endX) {
                savedSquares[idx] = board[castleY][x];
                board[castleY][x] = tempStart;
            }
            if (isWhiteMove) {
                whiteKingMoved = true;
            } else {
                blackKingMoved = true;
            }
            if (IsKingInCheck(isWhiteMove)) {
                // Undo the king move and reset moved state
                board[castleY][startX] = tempStart;
                if (x == endX) board[castleY][x] = savedSquares[idx];
                if (isWhiteMove) {
                    whiteKingMoved = previouslyMoved;
                } else {
                    blackKingMoved = previouslyMoved;
                }
                return false;
            }
            // Undo the king move
            board[castleY][startX] = tempStart;
            if (x == endX) board[castleY][x] = savedSquares[idx];
            idx++;
        }
        // reset king move state before returning true
        if (isWhiteMove) {
            whiteKingMoved = previouslyMoved;
        } else {
            blackKingMoved = previouslyMoved;
        }
    }

    // --- En passant legality: temporarily remove captured pawn ---
    bool isEnPassant = (
        tolower(tempStart) == 'p' &&
        endX == enPassantCaptureSquare.x &&
        endY == enPassantCaptureSquare.y &&
        abs(startX - endX) == 1 &&
        ((isupper(tempStart) && startY == 3) || (!isupper(tempStart) && startY == 4)) &&
        tempEnd == ' '
    );
    char capturedPawn = 0;
    if (isEnPassant) {
        int captureRow = isupper(tempStart) ? 3 : 4;
        capturedPawn = board[captureRow][endX];
        board[startY][startX] = ' ';
        board[captureRow][endX] = ' ';
        board[endY][endX] = tempStart;
    } else {
        // Make the move temporarily
        board[startY][startX] = ' ';
        board[endY][endX] = tempStart;
    }

    bool isPlayerKingInCheck = IsKingInCheck(isWhiteMove);  // Check if player's own king is in check

    // Revert the move
    if (isEnPassant) {
        board[startY][startX] = tempStart;
        board[endY][endX] = tempEnd;
        int captureRow = isupper(tempStart) ? 3 : 4;
        board[captureRow][endX] = capturedPawn;
    } else {
        board[startY][startX] = tempStart;
        board[endY][endX] = tempEnd;
    }

    if (isPlayerKingInCheck) {
        return false; // The move leaves player's own king in check
    }

    return true; // The move is valid
}

// --- Promotion state ---
bool promotionPending = false;
int promotionFromX = -1, promotionFromY = -1, promotionToX = -1, promotionToY = -1;
char promotionColor = 0; // 'w' or 'b'
char promotionChosen = 0; // 'Q', 'R', 'B', 'N' (uppercase for white, lowercase for black)

// Helper to print move in algebraic notation
// void PrintMove(int startX, int startY, int endX, int endY, char movedPiece, char promotionPiece) {
//     // --- Store move for PGN output ---
//     // if (pgnMoveCount < MAX_MOVES) {
//     //     PGNMove *m = &pgnMoves[pgnMoveCount++];
//     //     m->startX = startX; m->startY = startY; m->endX = endX; m->endY = endY;
//     //     m->piece = movedPiece;
//     //     m->promotion = promotionPiece;
//     //     m->isWhite = isupper(movedPiece);
//     //     m->moveNumber = (pgnMoveCount+1)/2;
//     //     m->captured = board[endY][endX];
//     // }
//     char files[] = "abcdefgh";
//     char moveStr[8];
//     if (tolower(movedPiece) == 'p' && ((isupper(movedPiece) && endY == 0) || (!isupper(movedPiece) && endY == 7)) && promotionPiece) {
//         // e7e8q
//         snprintf(moveStr, sizeof(moveStr), "%c%d%c%d%c",
//             files[startX], 8 - startY,
//             files[endX], 8 - endY,
//             tolower(promotionPiece));
//     } else {
//         // e2e4
//         snprintf(moveStr, sizeof(moveStr), "%c%d%c%d",
//             files[startX], 8 - startY,
//             files[endX], 8 - endY);
//     }
//     printf("%s\n", moveStr);
//     // --- Store move for PGN output ---
//     // if (pgnMoveCount < MAX_MOVES) {
//     //     snprintf(pgnMoves[pgnMoveCount++], 8, "%s", moveStr);
//     // }
// }

// Add a flag to suppress double printing for AI
// bool suppressPrintMove = false;

void ExecuteMove(char piece, int startX, int startY, int endX, int endY, char promotionPiece) {
    // --- Gather pre-move info for PGN ---
    char capturedPiece = board[endY][endX];
    bool isCapture = (capturedPiece != ' ');
    bool isEnPassant = (
        tolower(piece) == 'p' &&
        endX == enPassantCaptureSquare.x &&
        endY == enPassantCaptureSquare.y &&
        abs(startX - endX) == 1 &&
        ((isupper(piece) && startY == 3) || (!isupper(piece) && startY == 4)) &&
        board[endY][endX] == ' '
    );
    if (isEnPassant) {
        int captureRow = isupper(piece) ? 3 : 4;
        capturedPiece = board[captureRow][endX];
    }
    bool isCastle = false, isCastleLong = false;
    if (tolower(piece) == 'k' && abs(startX - endX) == 2) {
        isCastle = true;
        isCastleLong = (endX < startX);
    }

    // Record BEFORE changing the board (for disambiguation logic)
    PGN_RecordMoveBefore(startX,startY,endX,endY,piece,capturedPiece,promotionPiece,
                         isCapture || isEnPassant,isCastle,isCastleLong,isEnPassant);

    // --- Sounds (unchanged except we reuse isCapture/isEnPassant) ---
    if (isCapture || isEnPassant) PlaySound(captureSound);
    else PlaySound(moveSound);

    // Half-move clock update
    if (tolower(piece) == 'p' || isCapture || isEnPassant) halfMoveClock = 0;
    else halfMoveClock++;

    // Perform en passant capture on board
    if (isEnPassant) {
        int captureRow = isupper(piece) ? 3 : 4;
        board[captureRow][endX] = ' ';
        halfMoveClock = 0;
    }

    // Set/reset en passant square
    if (tolower(piece) == 'p' && abs(startY - endY) == 2) {
        enPassantCaptureSquare = (Vector2){endX, startY + (isupper(piece) ? -1 : 1)};
    } else {
        enPassantCaptureSquare = (Vector2){-1, -1};
    }

    // Promotion
    if (tolower(piece) == 'p') {
        if ((isupper(piece) && endY == 0) || (!isupper(piece) && endY == 7)) {
            if (promotionPiece) piece = promotionPiece;
            else piece = isupper(piece) ? 'Q' : 'q';
            halfMoveClock = 0;
        }
    }

    // Castling rook move
    if (tolower(piece) == 'k' && abs(startX - endX) == 2) {
        if (isupper(piece)) {
            if (endX == 6) { board[7][5] = 'R'; board[7][7] = ' '; whiteRookMoved[1] = true; }
            else { board[7][3] = 'R'; board[7][0] = ' '; whiteRookMoved[0] = true; }
        } else {
            if (endX == 6) { board[0][5] = 'r'; board[0][7] = ' '; blackRookMoved[1] = true; }
            else { board[0][3] = 'r'; board[0][0] = ' '; blackRookMoved[0] = true; }
        }
    }

    // Execute main move
    board[startY][startX] = ' ';
    board[endY][endX] = piece;

    // Toggle turn
    isWhiteTurn = !isWhiteTurn;

    // Update castling rights etc.
    UpdateCastlingRights(piece, startX, startY, endX, endY);
    lastEnd.x = endX; lastEnd.y = endY;

    // --- Determine check / mate for SAN suffix ---
    bool opponentIsWhite = isWhiteTurn; // after toggle
    bool opponentInCheck = IsKingInCheck(opponentIsWhite);
    bool opponentHasLegal = false;
    if (opponentInCheck) {
        // test for any legal escape
        for (int y=0; y<8 && !opponentHasLegal; y++) {
            for (int x=0; x<8 && !opponentHasLegal; x++) {
                if ((opponentIsWhite && isupper(board[y][x])) || (!opponentIsWhite && islower(board[y][x]))) {
                    int mc; Vector2 *pm = GeneratePieceMoves(board[y][x], x, y, &mc);
                    for (int i=0;i<mc && !opponentHasLegal;i++) {
                        int tx = (int)pm[i].x, ty=(int)pm[i].y;
                        if (IsValidMove(x,y,tx,ty)) opponentHasLegal = true;
                    }
                    free(pm);
                }
            }
        }
    }
    PGN_FinalizeLastMove(opponentInCheck, (opponentInCheck && !opponentHasLegal));
}


bool IsInsufficientMaterial() {
    int knights[2] = {0, 0}, bishops[2] = {0, 0};
    bool differentBishops = false;
    int pieces = 0;

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            char piece = board[y][x];
            if (piece != ' ' && tolower(piece) != 'k') {
                pieces++;
                if (tolower(piece) == 'n') {
                    knights[isupper(piece) ? 0 : 1]++;
                } else if (tolower(piece) == 'b') {
                    bishops[isupper(piece) ? 0 : 1]++;
                    differentBishops |= ((x + y) % 2 == 0);
                }
            }
        }
    }

    return pieces == 0 || 
           (pieces == 1 && (knights[0] == 1 || knights[1] == 1 || bishops[0] == 1 || bishops[1] == 1)) ||
           (pieces == 2 && differentBishops && bishops[0] == 1 && bishops[1] == 1);
}

bool IsFiftyMoveRule() {
    return halfMoveClock >= 100; // 50 full moves, each consisting of a move by both players
}

bool IsSeventyFiveMoveRule() {
    return halfMoveClock >= 150; // 75 full moves
}


void UpdateCastlingRights(char piece, int startX, int startY, int endX, int endY) {
    // Update castling rights based on the moved piece
    if (tolower(piece) == 'k') {
        if (isupper(piece)) {
            whiteKingMoved = true;
        } else {
            blackKingMoved = true;
        }
    } else if (tolower(piece) == 'r') {
        if (startX == 0 && startY == (isupper(piece) ? 7 : 0)) {
            if (isupper(piece)) whiteRookMoved[0] = true;
            else blackRookMoved[0] = true;
        } else if (startX == 7 && startY == (isupper(piece) ? 7 : 0)) {
            if (isupper(piece)) whiteRookMoved[1] = true;
            else blackRookMoved[1] = true;
        }
    }
}

void HandlePromotionPopup() {
    if (!promotionPending) return;
    int w = 320, h = 180;
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    Rectangle box = {x, y, w, h};
    int result = GuiMessageBox(box, "Pawn Promotion", "Choose piece to promote to:", "Queen;Rook;Bishop;Knight");
    if (result > 0) {
        char promo = 0;
        if (promotionColor == 'w') {
            if (result == 1) promo = 'Q';
            if (result == 2) promo = 'R';
            if (result == 3) promo = 'B';
            if (result == 4) promo = 'N';
        } else {
            if (result == 1) promo = 'q';
            if (result == 2) promo = 'r';
            if (result == 3) promo = 'b';
            if (result == 4) promo = 'n';
        }
        ExecuteMove(board[promotionFromY][promotionFromX], promotionFromX, promotionFromY, promotionToX, promotionToY, promo);
        promotionPending = false;
        promotionFromX = promotionFromY = promotionToX = promotionToY = -1;
        promotionColor = 0;
        promotionChosen = 0;
    }
}

// --- Classic chess input: click-to-select, click-to-move, drag-to-move (immediate drag) ---
void HandleInput() {
    int squareSize = chessBoardSize / 8;
    if (promotionPending) return; // Block input during promotion
    Vector2 mousePos = GetMousePosition();
    int x = (mousePos.x - boardOffsetX) / squareSize;
    int y = (mousePos.y - boardOffsetY) / squareSize;

    // --- Dragging logic: allow immediate drag ---
    static bool isDragInitiated = false;
    static Vector2 dragStartPos = {0};
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragStartPos = mousePos;
        isDragInitiated = false;
        // If you press on your own piece, start dragging immediately
        if (x >= 0 && x < 8 && y >= 0 && y < 8 && board[y][x] != ' ' && ((isWhiteTurn && isupper(board[y][x])) || (!isWhiteTurn && islower(board[y][x])))) {
            isDragging = true;
            dragPieceX = x;
            dragPieceY = y;
            dragOffset.x = dragStartPos.x - positions[y][x].x;
            dragOffset.y = dragStartPos.y - positions[y][x].y;
            lastStart.x = x;
            lastStart.y = y;
            isDragInitiated = true;
        }
    }
    if (isDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        positions[dragPieceY][dragPieceX] = (Vector2){ mousePos.x - dragOffset.x, mousePos.y - dragOffset.y };
    }
    if (isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        isDragging = false;
        int endX = (mousePos.x - boardOffsetX) / squareSize;
        int endY = (mousePos.y - boardOffsetY) / squareSize;
        if (endX >= 0 && endX < 8 && endY >= 0 && endY < 8 && IsValidMove(dragPieceX, dragPieceY, endX, endY)) {
            char piece = board[dragPieceY][dragPieceX];
            if (tolower(piece) == 'p' && ((isupper(piece) && endY == 0) || (!isupper(piece) && endY == 7))) {
                promotionPending = true;
                promotionFromX = dragPieceX;
                promotionFromY = dragPieceY;
                promotionToX = endX;
                promotionToY = endY;
                promotionColor = isupper(piece) ? 'w' : 'b';
                return;
            } else {
                ExecuteMove(piece, dragPieceX, dragPieceY, endX, endY, 0);
            }
            lastEnd.x = endX;
            lastEnd.y = endY;
            if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
            selectedPieceX = selectedPieceY = -1;
            selectedMoveCount = 0;
        }
        // If not a valid move, keep selection/highlight
    }

    // --- Click-to-select and click-to-move logic ---
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDragging) {
        // Clicked outside board: deselect
        if (x < 0 || x >= 8 || y < 0 || y >= 8) {
            if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
            selectedPieceX = selectedPieceY = -1;
            selectedMoveCount = 0;
            return;
        }
        // If a piece is already selected
        if (selectedPieceX != -1 && selectedPieceY != -1 && selectedMoves != NULL) {
            // Clicked the same piece again: deselect
            if (x == selectedPieceX && y == selectedPieceY) {
                if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
                selectedPieceX = selectedPieceY = -1;
                selectedMoveCount = 0;
                return;
            }
            // Clicked a valid move square: move the piece
            for (int i = 0; i < selectedMoveCount; i++) {
                if ((int)selectedMoves[i].x == x && (int)selectedMoves[i].y == y) {
                    if (IsValidMove(selectedPieceX, selectedPieceY, x, y)) {
                        char piece = board[selectedPieceY][selectedPieceX];
                        if (tolower(piece) == 'p' && ((isupper(piece) && y == 0) || (!isupper(piece) && y == 7))) {
                            promotionPending = true;
                            promotionFromX = selectedPieceX;
                            promotionFromY = selectedPieceY;
                            promotionToX = x;
                            promotionToY = y;
                            promotionColor = isupper(piece) ? 'w' : 'b';
                            return;
                        } else {
                            ExecuteMove(piece, selectedPieceX, selectedPieceY, x, y, 0);
                        }
                        lastEnd.x = x;
                        lastEnd.y = y;
                        if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
                        selectedPieceX = selectedPieceY = -1;
                        selectedMoveCount = 0;
                        return;
                    }
                }
            }
            // Clicked another of your own pieces: switch selection
            if (board[y][x] != ' ' && ((isWhiteTurn && isupper(board[y][x])) || (!isWhiteTurn && islower(board[y][x])))) {
                if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
                selectedPieceX = x;
                selectedPieceY = y;
                selectedMoves = GeneratePieceMoves(board[y][x], x, y, &selectedMoveCount);
                return;
            }
            // Clicked elsewhere: deselect
            if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
            selectedPieceX = selectedPieceY = -1;
            selectedMoveCount = 0;
            return;
        }
        // No piece selected: select if it's your own piece
        if (board[y][x] != ' ' && ((isWhiteTurn && isupper(board[y][x])) || (!isWhiteTurn && islower(board[y][x])))) {
            if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
            selectedPieceX = x;
            selectedPieceY = y;
            selectedMoves = GeneratePieceMoves(board[y][x], x, y, &selectedMoveCount);
        } else {
            // Clicked on empty square or opponent's piece: deselect
            if (selectedMoves) { free(selectedMoves); selectedMoves = NULL; }
            selectedPieceX = selectedPieceY = -1;
            selectedMoveCount = 0;
        }
    }
}

void Mode() {
    HandleInput();
    // For example, handling AI moves if it's the AI's turn
    if ((currentMode == MODE_PLAY_WHITE && !isWhiteTurn) ||
        (currentMode == MODE_PLAY_BLACK && isWhiteTurn) ||
        currentMode == MODE_AI_VS_AI) {
        AI_PlayMove(isWhiteTurn);
    }
}

// Helper to print game mode
void PrintGameMode(void) {
    const char* modeStr = "";
    switch (currentMode) {
        case MODE_NORMAL: modeStr = "Normal Mode"; break;
        case MODE_PLAY_WHITE: modeStr = "Play as White"; break;
        case MODE_PLAY_BLACK: modeStr = "Play as Black"; break;
        case MODE_AI_VS_AI: modeStr = "AI vs AI"; break;
    }
    printf("\n==============================\n");
    printf("New Game: %s\n", modeStr);
    printf("==============================\n");
}



// Helper: convert move to SAN
// Helper: copy board
// Print full PGN with headers and SAN moves
// Print full PGN with headers and SAN moves

// Perft (move counting) function for debugging
long long perft(int depth) {
    if (depth == 0) return 1;
    long long nodes = 0;
    int moveCount;
    // Save board state for undo
    char boardCopy[8][8];
    memcpy(boardCopy, board, sizeof(board));
    bool isWhiteTurnCopy = isWhiteTurn;
    bool whiteKingMovedCopy = whiteKingMoved, blackKingMovedCopy = blackKingMoved;
    bool whiteRookMovedCopy[2] = {whiteRookMoved[0], whiteRookMoved[1]};
    bool blackRookMovedCopy[2] = {blackRookMoved[0], blackRookMoved[1]};
    Vector2 enPassantCopy = enPassantCaptureSquare;
    int halfMoveClockCopy = halfMoveClock;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            char piece = board[y][x];
            if (piece == ' ') continue;
            if ((isWhiteTurn && isupper(piece)) || (!isWhiteTurn && islower(piece))) {
                Vector2 *moves = GeneratePieceMoves(piece, x, y, &moveCount);
                for (int i = 0; i < moveCount; i++) {
                    int tx = (int)moves[i].x, ty = (int)moves[i].y;
                    if (!IsValidMove(x, y, tx, ty)) continue;
                    // Save move info for undo
                    char moved = board[y][x];
                    // Promotion handling
                    char promo = 0;
                    if (tolower(moved) == 'p' && ((isupper(moved) && ty == 0) || (!isupper(moved) && ty == 7))) {
                        promo = isupper(moved) ? 'Q' : 'q';
                    }
                    // Make the move
                    ExecuteMove(moved, x, y, tx, ty, promo);
                    // Recurse
                    nodes += perft(depth - 1);
                    // Undo the move (restore all state)
                    memcpy(board, boardCopy, sizeof(board));
                    isWhiteTurn = isWhiteTurnCopy;
                    whiteKingMoved = whiteKingMovedCopy;
                    blackKingMoved = blackKingMovedCopy;
                    whiteRookMoved[0] = whiteRookMovedCopy[0];
                    whiteRookMoved[1] = whiteRookMovedCopy[1];
                    blackRookMoved[0] = blackRookMovedCopy[0];
                    blackRookMoved[1] = blackRookMovedCopy[1];
                    enPassantCaptureSquare = enPassantCopy;
                    halfMoveClock = halfMoveClockCopy;
                }
                free(moves);
            }
        }
    }
    return nodes;
}


int main(void) {
    InitWindow(screenWidth, screenHeight, "Chess Engine");
    InitAudioDevice(); // Initialize audio device
    moveSound = LoadSound("sounds/move.wav");
    captureSound = LoadSound("sounds/capture.wav");
    char fenString[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    float boardScale = 0.8f;
    chessBoardSize = (int)((screenWidth < screenHeight ? screenWidth : screenHeight) * boardScale);
    boardOffsetX = (screenWidth - chessBoardSize) / 2;
    boardOffsetY = (screenHeight - chessBoardSize) / 2;
    Image chessImage = LoadImage("images/chess.png");
    SetWindowIcon(chessImage);
    UnloadImage(chessImage);
    LoadChessPieces();
    SetupBoardFromFEN(fenString);
    PGN_Reset();
    PrintBoard(); // Debug: print board after FEN setup
    CheckKingsCount();
    // Perft test output
    printf("Perft(1): %lld\n", perft(1));
    printf("Perft(2): %lld\n", perft(2));
    printf("Perft(3): %lld\n", perft(3));
    printf("Perft(4): %lld\n", perft(4));
    AI_Init(4);   // depth‑4 search – raise for a stronger engine
    SetTargetFPS(60);
    srand(time(NULL));
    // ToggleFullscreen();
    

    // Main game loop
    while (!WindowShouldClose()) {
        // Handle window resizing
        if (IsWindowResized()) {
            int newWidth = GetScreenWidth();
            int newHeight = GetScreenHeight();
            chessBoardSize = (int)((newWidth < newHeight ? newWidth : newHeight) * boardScale);
            boardOffsetX = (newWidth - chessBoardSize) / 2;
            boardOffsetY = (newHeight - chessBoardSize) / 2;
        }
        // Update game logic
        if (!gameOver) {
            if (currentMode != MODE_AI_VS_AI) {
                HandleInput(); // <--- Unified input handler
            }

            // AI's turn to play in AI vs AI mode, or in single-player modes
            if ((currentMode == MODE_PLAY_WHITE && !isWhiteTurn) ||
                (currentMode == MODE_PLAY_BLACK && isWhiteTurn) ||
                currentMode == MODE_AI_VS_AI) {
                AI_PlayMove(isWhiteTurn);
            }
        }
        // Drawing
        BeginDrawing();
        ClearBackground(DGRAY);
        DrawChessBoard();
        DrawPieces();
        HandlePromotionPopup();


        int buttonHeight = 50;
        int buttonWidth = 150; // Set the button width to the desired value.
        int startY = screenHeight / 2 - (buttonHeight * 2 + 10); // Adjust as needed for spacing.

        // Normal Mode button
        if (GuiButton((Rectangle){10, startY, buttonWidth, buttonHeight}, "Normal Mode")) {
            if (currentMode != MODE_NORMAL) {
                currentMode = MODE_NORMAL;
                SetupBoardFromFEN(fenString);
                PGN_Reset();
                isDragging = false; dragPieceX = dragPieceY = -1; // Reset drag state
                PrintGameMode();
            }
        }

        // Play as White button
        if (GuiButton((Rectangle){10, startY + buttonHeight + 10, buttonWidth, buttonHeight}, "Play as White")) {
            if (currentMode != MODE_PLAY_WHITE) {
                currentMode = MODE_PLAY_WHITE;
                SetupBoardFromFEN(fenString);
                PGN_Reset();
                isDragging = false; dragPieceX = dragPieceY = -1; // Reset drag state
                PrintGameMode();
            }
        }

        // Play as Black button
        if (GuiButton((Rectangle){10, startY + 2 * (buttonHeight + 10), buttonWidth, buttonHeight}, "Play as Black")) {
            if (currentMode != MODE_PLAY_BLACK) {
                currentMode = MODE_PLAY_BLACK;
                SetupBoardFromFEN(fenString);
                PGN_Reset();
                isDragging = false; dragPieceX = dragPieceY = -1; // Reset drag state
                PrintGameMode();
            }
        }

        // AI vs AI button
        if (GuiButton((Rectangle){10, startY + 3 * (buttonHeight + 10), buttonWidth, buttonHeight}, "AI vs AI")) {
            if (currentMode != MODE_AI_VS_AI) {
                currentMode = MODE_AI_VS_AI;
                SetupBoardFromFEN(fenString);
                PGN_Reset();
                isDragging = false; dragPieceX = dragPieceY = -1; // Reset drag state
                PrintGameMode();
            }
        }

        // Restart button
        if (GuiButton((Rectangle){10, screenHeight - 160, buttonWidth, buttonHeight}, "Restart")) {
            SetupBoardFromFEN(fenString);
            PGN_Reset();
            halfMoveClock = 0;
            isWhiteTurn = true;
            whiteKingMoved = blackKingMoved = false;
            for (int i = 0; i < 2; i++) {
                whiteRookMoved[i] = blackRookMoved[i] = false;
            }
            gameOver = false;
            isDragging = false; dragPieceX = dragPieceY = -1; // Reset drag state
            PrintGameMode();
            // --- Reset PGN move list ---
            // pgnMoveCount = 0; // Removed
        }

        // Quit button
        if (GuiButton((Rectangle){10, screenHeight - 100, buttonWidth, buttonHeight}, "Quit")) {
            CloseWindow();
            return 0;
        }

        // Print PGN button
        if (GuiButton((Rectangle){10, screenHeight - 220, buttonWidth, buttonHeight}, "Print PGN")) {
            PGN_PrintGame();
        }
        // Print PGN on G key
        // if (IsKeyPressed(KEY_G)) { // Removed
        //     PrintFullPGN(); // Removed
        // }


        // --- LEGAL MOVE COUNTING FOR GAME OVER ---
        int totalMoveCount = 0;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if ((isWhiteTurn && isupper(board[y][x])) || (!isWhiteTurn && islower(board[y][x]))) {
                    int moveCount;
                    Vector2 *pieceMoves = GeneratePieceMoves(board[y][x], x, y, &moveCount);
                    for (int i = 0; i < moveCount; i++) {
                        if (IsValidMove(x, y, (int)pieceMoves[i].x, (int)pieceMoves[i].y)) {
                            totalMoveCount++;
                        }
                    }
                    free(pieceMoves);
                }
            }
        }
        if (totalMoveCount == 0) {
            gameOver = true;
            bool whiteKingInCheck = IsKingInCheck(true);
            bool blackKingInCheck = IsKingInCheck(false);
            if ((isWhiteTurn && whiteKingInCheck) || (!isWhiteTurn && blackKingInCheck)) {
                gameOverMessage = isWhiteTurn ? "Black wins by checkmate!" : "White wins by checkmate!";
                PGN_SetResult(isWhiteTurn ? "0-1" : "1-0");
            } else {
                gameOverMessage = "Stalemate!"; // Stalemate condition
                PGN_SetResult("1/2-1/2");
            }
        } else if (IsFiftyMoveRule() || IsSeventyFiveMoveRule()) {
            gameOver = true;
            gameOverMessage = "Draw by move rule!";
            PGN_SetResult("1/2-1/2");
        } else if (IsInsufficientMaterial()) {
            gameOver = true;
            gameOverMessage = "Draw by insufficient material!";
            PGN_SetResult("1/2-1/2");
        }
        // Always display game over message regardless of mode
        if (gameOver) {
            int textWidth = MeasureText(gameOverMessage, 20);
            int textHeight = 20;
            int posX = GetScreenWidth() - textWidth - 10;
            int posY = GetScreenHeight() - textHeight - 10;
            DrawText(gameOverMessage, posX, posY, 20, RED);
        }

        EndDrawing();
    }
    UnloadSound(moveSound);
    UnloadSound(captureSound);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}

