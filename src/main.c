#define RAYGUI_IMPLEMENTATION
#include "raylib.h"
#include "raygui.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

int boardOffsetX, boardOffsetY;
const int screenWidth = 1920;
const int screenHeight = 1080;
const int chessBoardSize = 800;

const Color DGRAY = {35, 38,46, 1}; 
const Color WHITE_SQUARE = {215, 192, 162, 255};
const Color BLACK_SQUARE = {131, 105, 83, 255};
const Color START_POS_COLOR = {255, 214, 10, 128};  
const Color END_POS_COLOR = {255, 195, 0, 128};  

int halfMoveClock = 0; // To track the number of half-moves (each player's turn is a half-move)
bool isStalemate = false;
bool isWhiteTurn = true; // White starts
bool whiteKingMoved = false, blackKingMoved = false;
bool whiteRookMoved[2] = {false, false}, blackRookMoved[2] = {false, false};

bool IsKingInCheck(bool isWhiteKing);
bool IsOpponentPiece(char piece, int x, int y);
bool IsValidMove(int startX, int startY, int endX, int endY);
void ExecuteMove(char piece, int startX, int startY, int endX, int endY);
Vector2 *GeneratePieceMoves(char piece, int startX, int startY, int *moveCount);
void UpdateCastlingRights(char piece, int startX, int startY, int endX, int endY);

void LoadChessPieces() {
    Image image = LoadImage("images/chesspieces.png");
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

            // Highlight possible moves for the selected piece in light blue
            // After the loops that draw the squares
            if (isDragging) {
                int moveCount;
                Vector2 *moves = GeneratePieceMoves(board[dragPieceY][dragPieceX], dragPieceX, dragPieceY, &moveCount);
                for (int i = 0; i < moveCount; i++) {
                    int moveX = (int)moves[i].x;
                    int moveY = (int)moves[i].y;
                    if (IsValidMove(dragPieceX, dragPieceY, moveX, moveY)) {
                        Rectangle moveRect = {boardOffsetX + moveX * squareSize, boardOffsetY + moveY * squareSize, squareSize, squareSize};
                        Color lightBlue = {173, 216, 230, 10};// Light blue color
                        DrawRectangleRec(moveRect, lightBlue);
                    }
                }
                free(moves);
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
    Rectangle sourceRect = { 0.0f, 0.0f, 300.0f, 300.0f };
    int row = (piece >= 'a' && piece <= 'z') ? 1 : 0;
    int col;
    switch (tolower(piece)) {
        case 'k': col = 0; break;
        case 'q': col = 1; break;
        case 'r': col = 4; break;
        case 'n': col = 3; break;
        case 'b': col = 2; break;
        case 'p': col = 5; break;
        default:  return;
    }
    sourceRect.x = col * 300.0f;
    sourceRect.y = row * 300.0f;

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

void SetupBoardFromFEN(const char* fen) {
    memset(board, ' ', sizeof(board)); // Initialize the board with empty spaces
    int x = 0, y = 0;
    while (*fen) {
        if (*fen == '/') {
            y++;
            x = 0;
        } else if (isdigit(*fen)) {
            for (int j = 0; j < (*fen - '0'); j++) {
                board[y][x + j] = ' ';
            }
            x += (*fen - '0');
        } else {
            board[y][x] = *fen;
            x++;
        }
        fen++;
    }
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

bool IsOpponentPiece(char piece, int x, int y) {
    if (board[y][x] == ' ') return false; // Empty square
    return isupper(piece) != isupper(board[y][x]);
}


Vector2 *GeneratePieceMoves(char piece, int startX, int startY, int *moveCount) {
    Vector2 *moves = NULL;
    *moveCount = 0;

    // Function to add a move to the moves array
    void addMove(int x, int y) {
        if (x >= 0 && x < 8 && y >= 0 && y < 8) {
            if (board[y][x] == ' ' || IsOpponentPiece(piece, x, y)) {
                moves = realloc(moves, (*moveCount + 1) * sizeof(Vector2));
                moves[*moveCount] = (Vector2){x, y};
                (*moveCount)++;
            }
        }
    }

     // Function to add line moves (for rook, bishop, queen)
    void addLineMoves(int dx, int dy) {
        int x = startX + dx;
        int y = startY + dy;
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            if (board[y][x] != ' ' && !IsOpponentPiece(piece, x, y)) break;
            addMove(x, y);
            if (board[y][x] != ' ') break; // Stop if hit a piece
            x += dx;
            y += dy;
        }
    }
    switch (tolower(piece)) {
        case 'p': { // Pawn
            int direction = isupper(piece) ? -1 : 1;  // Define direction once for the pawn
            // Forward move
            if (board[startY + direction][startX] == ' ') {
                addMove(startX, startY + direction);
                // Initial two-square move
                if ((isupper(piece) && startY == 6) || (!isupper(piece) && startY == 1)) {
                    if (board[startY + 2 * direction][startX] == ' ') {
                        addMove(startX, startY + 2 * direction);
                    }
                }
            }
            // Captures
            if (IsOpponentPiece(piece, startX - 1, startY + direction)) {
                addMove(startX - 1, startY + direction);
            }
            if (IsOpponentPiece(piece, startX + 1, startY + direction)) {
                addMove(startX + 1, startY + direction);
            }

            // En passant capture
            if (startY == (isupper(piece) ? 3 : 4)) {
                if (enPassantCaptureSquare.x == startX - 1 && enPassantCaptureSquare.y == startY + direction) {
                    addMove(startX - 1, startY + direction);
                }
                if (enPassantCaptureSquare.x == startX + 1 && enPassantCaptureSquare.y == startY + direction) {
                    addMove(startX + 1, startY + direction);
                }
            }
        
            break;
        }

        case 'n': { // Knight
            int knightMoves[8][2] = {{-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};
            for (int i = 0; i < 8; i++) {
                int x = startX + knightMoves[i][0];
                int y = startY + knightMoves[i][1];
                if ((board[y][x] == ' ') || IsOpponentPiece(piece, x, y)) {
                    addMove(x, y);
                }
            }
            break;
        }
        case 'r': { // Rook
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 || dy == 0) addLineMoves(dx, dy);
                }
            }
            break;
        }
        case 'b': { // Bishop
            for (int dx = -1; dx <= 1; dx += 2) {
                for (int dy = -1; dy <= 1; dy += 2) {
                    addLineMoves(dx, dy);
                }
            }
            break;
        }
        case 'q': { // Queen
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx != 0 || dy != 0) addLineMoves(dx, dy);
                }
            }
            break;
        }
        case 'k': { // King
            // for normal king moves
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx != 0 || dy != 0) addMove(startX + dx, startY + dy);
                }
            }
            // Castling logic
            if (tolower(piece) == 'k') { // White king
                if (!whiteKingMoved) {
                    // Castling kingside (short castling)
                    if (!whiteRookMoved[1] && board[startY][5] == ' ' && board[startY][6] == ' ') {
                        addMove(6, startY);
                    }
                    // Castling queenside (long castling)
                    if (!whiteRookMoved[0] && board[startY][1] == ' ' && board[startY][2] == ' ' && board[startY][3] == ' ') {
                        addMove(2, startY);
                    }
                }
            } else if (tolower(piece) == 'k') { // Black king
                if (!blackKingMoved) {
                    // Castling kingside (short castling)
                    if (!blackRookMoved[1] && board[startY][5] == ' ' && board[startY][6] == ' ') {
                        addMove(6, startY);
                    }
                    // Castling queenside (long castling)
                    if (!blackRookMoved[0] && board[startY][1] == ' ' && board[startY][2] == ' ' && board[startY][3] == ' ') {
                        addMove(2, startY);
                    }
                }
            }
            break;
        }

    }
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

void CheckKingsCount() {
    int whiteKings = 0, blackKings = 0;

    // Count the number of white and black kings on the board
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (board[y][x] == 'K') {
                whiteKings++;
            } else if (board[y][x] == 'k') {
                blackKings++;
            }
        }
    }

   if (whiteKings > 1 || blackKings > 1) {
        printf("Error: Invalid king count detected!\n");
        }
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

    // Now check if the move does not leave player's own king in check
    char tempStart = board[startY][startX];
    char tempEnd = board[endY][endX];
    bool isWhiteMove = isupper(tempStart);

    // Check for castling conditions
    if (tolower(tempStart) == 'k' && abs(startX - endX) == 2) {
        int castleY = isWhiteMove ? 7 : 0;
        int direction = (endX > startX) ? 1 : -1;
        bool previouslyMoved = isWhiteMove ? whiteKingMoved : blackKingMoved; // store original state

        for (int x = startX + direction; x != endX + direction; x += direction) {
            if ((board[castleY][x] != ' ' && x != endX) || IsKingInCheck(isWhiteMove)) {
                return false; // The king is moving through or into check
            }
            // Temporarily move the king for check detection
            board[castleY][startX] = ' ';
            board[castleY][x] = tempStart;
            if (isWhiteMove) {
                whiteKingMoved = true;
            } else {
                blackKingMoved = true;
            }
            if (IsKingInCheck(isWhiteMove)) {
                // Undo the king move and reset moved state
                board[castleY][startX] = tempStart;
                board[castleY][x] = ' ';
                if (isWhiteMove) {
                    whiteKingMoved = previouslyMoved;
                } else {
                    blackKingMoved = previouslyMoved;
                }
                return false;
            }
            // Undo the king move
            board[castleY][startX] = tempStart;
            board[castleY][x] = ' ';
        }
        // reset king move state before returning true
        if (isWhiteMove) {
            whiteKingMoved = previouslyMoved;
        } else {
            blackKingMoved = previouslyMoved;
        }
    }

    // Make the move temporarily
    board[startY][startX] = ' ';
    board[endY][endX] = tempStart;

    bool isPlayerKingInCheck = IsKingInCheck(isWhiteMove);  // Check if player's own king is in check

    // Revert the move
    board[startY][startX] = tempStart;
    board[endY][endX] = tempEnd;

    if (isPlayerKingInCheck) {
        return false; // The move leaves player's own king in check
    }

    return true; // The move is valid
  

    
   // En passant validation
    if (tolower(board[startY][startX]) == 'p' && endX == enPassantCaptureSquare.x && endY == enPassantCaptureSquare.y) {
        int pawnDirection = isupper(board[startY][startX]) ? -1 : 1;
        int captureRow = startY;
        char capturedPawn = board[captureRow][endX];
        board[captureRow][endX] = ' '; // Temporarily remove the captured pawn

        // Make the temporary en passant move
        board[startY][startX] = ' ';
        board[endY][endX] = board[captureRow][endX];

        bool isPlayerKingInCheck = IsKingInCheck(isupper(board[startY][startX]));

        // Revert the board back to original
        board[startY][startX] = isupper(board[startY][startX]) ? 'P' : 'p';
        board[endY][endX] = ' ';
        board[captureRow][endX] = capturedPawn;

        if (isPlayerKingInCheck) return false; // King would be in check after en passant, so the move is not valid
    }
    


}

void PerformAIMove(bool isWhite) {
    int totalMoveCount;
    Vector2 *moves = GenerateMoves(isWhite, &totalMoveCount);

    if (totalMoveCount > 0) {
        int randomIndex = rand() % totalMoveCount;
        Vector2 selectedMove = moves[randomIndex];

        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if ((isWhite && isupper(board[y][x])) || (!isWhite && islower(board[y][x]))) {
                    int pieceMoveCount;
                    Vector2 *pieceMoves = GeneratePieceMoves(board[y][x], x, y, &pieceMoveCount);

                    for (int j = 0; j < pieceMoveCount; j++) {
                        if (pieceMoves[j].x == selectedMove.x && pieceMoves[j].y == selectedMove.y) {
                            if (IsValidMove(x, y, (int)selectedMove.x, (int)selectedMove.y)) {
                                char movedPiece = board[y][x];
                                ExecuteMove(movedPiece, x, y, (int)selectedMove.x, (int)selectedMove.y);
                                goto moveMade;
                            }
                        }
                    }
                    free(pieceMoves);
                }
            }
        }
    }

    moveMade:
    free(moves);
    CheckKingsCount();
}

void ExecuteMove(char piece, int startX, int startY, int endX, int endY) {
    // Check if the move is a pawn move or a capture
    if (tolower(piece) == 'p' || board[endY][endX] != ' ') {
        halfMoveClock = 0;
    } else {
        halfMoveClock++;
    }
    // Perform en passant capture
    if (tolower(piece) == 'p' && endX == enPassantCaptureSquare.x && abs(startY - endY) == 1) {
        int captureRow = isupper(piece) ? 3 : 4; // Opponent's pawn row
        board[captureRow][endX] = ' ';
    }
    // Set or reset en passant capture square for pawn moves
    if (tolower(piece) == 'p' && abs(startY - endY) == 2) { // Two-square move
        enPassantCaptureSquare = (Vector2){endX, startY + (isupper(piece) ? -1 : 1)};
    } else {
        enPassantCaptureSquare = (Vector2){-1, -1};
    }

    // Pawn promotion logic
    if (tolower(piece) == 'p') {
        if ((isupper(piece) && endY == 0) || (!isupper(piece) && endY == 7)) {
            // Promote pawn to queen
            piece = isupper(piece) ? 'Q' : 'q';
        }
    }

    // Execute the move
    board[startY][startX] = ' ';
    board[endY][endX] = piece;
    
    // Update the game state (turn, castling rights, etc.)
    isWhiteTurn = !isWhiteTurn;
    UpdateCastlingRights(piece, startX, startY, endX, endY);
    lastEnd.x = endX;
    lastEnd.y = endY;
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

void HandlePieceDragging() {
    int squareSize = chessBoardSize / 8;

    // Start dragging
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isDragging) {
        Vector2 mousePos = GetMousePosition();
        int x = (mousePos.x - boardOffsetX) / squareSize;
        int y = (mousePos.y - boardOffsetY) / squareSize;

        if (board[y][x] != ' ' && ((isWhiteTurn && isupper(board[y][x])) || (!isWhiteTurn && islower(board[y][x])))) {
            isDragging = true;
            dragPieceX = x;
            dragPieceY = y;
            dragOffset.x = mousePos.x - positions[y][x].x;
            dragOffset.y = mousePos.y - positions[y][x].y;
            lastStart.x = x;
            lastStart.y = y;
        }
    }

    // Continue dragging
    if (isDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        positions[dragPieceY][dragPieceX] = (Vector2){ mousePos.x - dragOffset.x, mousePos.y - dragOffset.y };
    }

    // End dragging
    if (isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        isDragging = false;
        Vector2 mousePos = GetMousePosition();
        int endX = (mousePos.x - boardOffsetX) / squareSize;
        int endY = (mousePos.y - boardOffsetY) / squareSize;

        if (IsValidMove(dragPieceX, dragPieceY, endX, endY)) {
            ExecuteMove(board[dragPieceY][dragPieceX], dragPieceX, dragPieceY, endX, endY);
            lastEnd.x = endX;
            lastEnd.y = endY;
        } else {
            // Revert to original position if the move is not valid
            lastEnd.x = dragPieceX;
            lastEnd.y = dragPieceY;
        }
    }
}

void Mode() {
    HandlePieceDragging();
    // For example, handling AI moves if it's the AI's turn
    if ((currentMode == MODE_PLAY_WHITE && !isWhiteTurn) ||
        (currentMode == MODE_PLAY_BLACK && isWhiteTurn) ||
        currentMode == MODE_AI_VS_AI) {
        PerformAIMove(isWhiteTurn);
    }
}

bool gameOver = false;
char* gameOverMessage = "";

int main(void) {
    InitWindow(screenWidth, screenHeight, "Chess Engine");
    char fenString[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    boardOffsetX = (screenWidth - chessBoardSize) / 2;
    boardOffsetY = (screenHeight - chessBoardSize) / 2;
    Image chessImage = LoadImage("images/chess.png");
    SetWindowIcon(chessImage);
    UnloadImage(chessImage);
    LoadChessPieces();
    SetupBoardFromFEN(fenString);
    CheckKingsCount();
    SetTargetFPS(60);
    srand(time(NULL));
    ToggleFullscreen();
    

    // Main game loop
    while (!WindowShouldClose()) {
        // Update game logic
        if (!gameOver) {
            if (currentMode != MODE_AI_VS_AI) {
                HandlePieceDragging();
            }

            // AI's turn to play in AI vs AI mode, or in single-player modes
            if ((currentMode == MODE_PLAY_WHITE && !isWhiteTurn) ||
                (currentMode == MODE_PLAY_BLACK && isWhiteTurn) ||
                currentMode == MODE_AI_VS_AI) {
                PerformAIMove(isWhiteTurn);
            }
        }
        // Drawing
        BeginDrawing();
        ClearBackground(DGRAY);
        DrawChessBoard();
        DrawPieces();

        


        int buttonHeight = 50;
        int buttonWidth = 150; // Set the button width to the desired value.
        int startY = screenHeight / 2 - (buttonHeight * 2 + 10); // Adjust as needed for spacing.

        // Normal Mode button
        if (GuiButton((Rectangle){10, startY, buttonWidth, buttonHeight}, "Normal Mode")) {
            if (currentMode != MODE_NORMAL) {
                currentMode = MODE_NORMAL;
                SetupBoardFromFEN(fenString);
            }
        }

        // Play as White button
        if (GuiButton((Rectangle){10, startY + buttonHeight + 10, buttonWidth, buttonHeight}, "Play as White")) {
            if (currentMode != MODE_PLAY_WHITE) {
                currentMode = MODE_PLAY_WHITE;
                SetupBoardFromFEN(fenString);
            }
        }

        // Play as Black button
        if (GuiButton((Rectangle){10, startY + 2 * (buttonHeight + 10), buttonWidth, buttonHeight}, "Play as Black")) {
            if (currentMode != MODE_PLAY_BLACK) {
                currentMode = MODE_PLAY_BLACK;
                SetupBoardFromFEN(fenString);
            }
        }

        // AI vs AI button
        if (GuiButton((Rectangle){10, startY + 3 * (buttonHeight + 10), buttonWidth, buttonHeight}, "AI vs AI")) {
            if (currentMode != MODE_AI_VS_AI) {
                currentMode = MODE_AI_VS_AI;
                SetupBoardFromFEN(fenString);
            }
        }

        // Restart button
        if (GuiButton((Rectangle){10, screenHeight - 160, buttonWidth, buttonHeight}, "Restart")) {
            SetupBoardFromFEN(fenString);
            halfMoveClock = 0;
            isWhiteTurn = true;
            whiteKingMoved = blackKingMoved = false;
            for (int i = 0; i < 2; i++) {
                whiteRookMoved[i] = blackRookMoved[i] = false;
            }
            gameOver = false;
        }

        // Quit button
        if (GuiButton((Rectangle){10, screenHeight - 100, buttonWidth, buttonHeight}, "Quit")) {
            CloseWindow();
            return 0;
        }


        if (!gameOver) {
            bool whiteKingInCheck = IsKingInCheck(true);
            bool blackKingInCheck = IsKingInCheck(false);

            int totalMoveCount;
            GenerateMoves(isWhiteTurn, &totalMoveCount);

            if (totalMoveCount == 0) {
                gameOver = true;
                if ((isWhiteTurn && whiteKingInCheck) || (!isWhiteTurn && blackKingInCheck)) {
                    gameOverMessage = isWhiteTurn ? "Black wins by checkmate!" : "White wins by checkmate!";
                } else {
                    gameOverMessage = "Stalemate!"; // Stalemate condition
                }
            } else if (IsFiftyMoveRule() || IsSeventyFiveMoveRule()) {
                gameOver = true;
                gameOverMessage = "Draw by move rule!";
            } else if (IsInsufficientMaterial()) {
                gameOver = true;
                gameOverMessage = "Draw by insufficient material!";
            }
        }


        if (gameOver) {
            // Display game over message

            int textWidth = MeasureText(gameOverMessage, 20);
            int textHeight = 20; // Assuming a font size of 20

            // Calculate the position for the bottom right corner
            int posX = GetScreenWidth() - textWidth - 10; // 10 pixels from the right edge
            int posY = GetScreenHeight() - textHeight - 10; // 10 pixels from the bottom edge

            // Draw the text at the calculated position
            DrawText(gameOverMessage, posX, posY, 20, RED);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}

