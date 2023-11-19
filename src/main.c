#define RAYGUI_IMPLEMENTATION
#include "raylib.h"
#include "C:\raygui-4.0\src\raygui.h"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

bool isWhiteTurn = true; // White starts
bool whiteKingMoved = false, blackKingMoved = false;
bool whiteRookMoved[2] = {false, false}, blackRookMoved[2] = {false, false};

bool IsKingInCheck(bool isWhiteKing);
bool IsOpponentPiece(char piece, int x, int y);
Vector2 *GeneratePieceMoves(char piece, int startX, int startY, int *moveCount);
bool IsValidMove(int startX, int startY, int endX, int endY);

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
    int x = 0, y = 0;
    for (int i = 0; fen[i]; i++) {
        if (fen[i] == '/') {
            y++;
            x = 0;
        } else if (fen[i] >= '1' && fen[i] <= '8') {
            for (int j = 0; j < fen[i] - '0'; j++) {
                board[y][x + j] = ' ';
            }
            x += fen[i] - '0';
        } else {
            board[y][x] = fen[i];
            x++;
        }
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


    // Check for each piece type
    switch (tolower(piece)) {
        case 'p': { // Pawn
            int direction = isupper(piece) ? -1 : 1;
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
}

void UpdateDragAndDrop() {
    int squareSize = chessBoardSize / 8;

    // Start dragging when the mouse button is pressed
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        // Adjust mouse position for board offsets
        int x = (mousePos.x - boardOffsetX) / squareSize;
        int y = (mousePos.y - boardOffsetY) / squareSize;

        // Check if a piece is at the clicked position and if it's the correct turn
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

    // Handle dragging movement
    if (isDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        // Update the dragged piece's position to follow the mouse cursor
        positions[dragPieceY][dragPieceX] = (Vector2){ mousePos.x - dragOffset.x, mousePos.y - dragOffset.y };
    }

    // Execute the move when the mouse button is released
    if (isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        isDragging = false; // Stop dragging
        Vector2 mousePos = GetMousePosition();
        // Adjust mouse position for board offsets
        int endX = (mousePos.x - boardOffsetX) / squareSize;
        int endY = (mousePos.y - boardOffsetY) / squareSize;

        // Check for castling conditions and normal moves
        if (IsValidMove(dragPieceX, dragPieceY, endX, endY)) {
            char draggedPiece = board[dragPieceY][dragPieceX];
        
            // Check for castling conditions
            if (tolower(draggedPiece) == 'k' && abs(dragPieceX - endX) == 2) {
                if (!isWhiteTurn && !blackKingMoved &&
                    ((endX == 2 && !blackRookMoved[0]) || (endX == 6 && !blackRookMoved[1])) &&
                    IsPathClear(dragPieceX, dragPieceY, endX, endY)) {
                    // Castling for black
                    int rookX = endX == 2 ? 0 : 7;
                    int newRookX = endX == 2 ? 3 : 5;
                    board[endY][endX] = draggedPiece;
                    board[endY][newRookX] = 'r';
                    board[endY][rookX] = ' ';
                    blackKingMoved = true;
                } else if (isWhiteTurn && !whiteKingMoved &&
                           ((endX == 2 && !whiteRookMoved[0]) || (endX == 6 && !whiteRookMoved[1])) &&
                           IsPathClear(dragPieceX, dragPieceY, endX, endY)) {
                    // Castling for white
                    int rookX = endX == 2 ? 0 : 7;
                    int newRookX = endX == 2 ? 3 : 5;
                    board[endY][endX] = draggedPiece;
                    board[endY][newRookX] = 'R';
                    board[endY][rookX] = ' ';
                    whiteKingMoved = true;
                }
            } else {
                // Normal moves
                board[endY][endX] = draggedPiece;
                if (tolower(draggedPiece) == 'r') {
                    if (isWhiteTurn) {
                        if (dragPieceX == 0) whiteRookMoved[0] = true;
                        if (dragPieceX == 7) whiteRookMoved[1] = true;
                    } else {
                        if (dragPieceX == 0) blackRookMoved[0] = true;
                        if (dragPieceX == 7) blackRookMoved[1] = true;
                    }
                } else if (tolower(draggedPiece) == 'k') {
                    if (isWhiteTurn) whiteKingMoved = true;
                    else blackKingMoved = true;
                }
            }

            board[dragPieceY][dragPieceX] = ' ';
            lastEnd.x = endX;
            lastEnd.y = endY;
            isWhiteTurn = !isWhiteTurn;
        } else {
            lastEnd.x = dragPieceX;
            lastEnd.y = dragPieceY;
        }
    }
}

// Add this function to generate a random legal move for black pieces
Vector2 GenerateRandomMove() {
    Vector2 move;
    int totalMoveCount;
    Vector2 *moves = GenerateMoves(false, &totalMoveCount); // Generate moves for black pieces
    if (totalMoveCount > 0) {
        int randomIndex = GetRandomValue(0, totalMoveCount - 1);
        move = moves[randomIndex];
    } else {
        // Handle the case where there are no legal moves for black
        move = (Vector2){-1, -1}; // Invalid move
    }
    free(moves);
    return move;
}

int GetRandomNumber(int min, int max) {
    return min + rand() % (max - min + 1);
}


int main(void) {

    InitWindow(screenWidth, screenHeight, "Chess Engine");
    boardOffsetX = (screenWidth - chessBoardSize) / 2;
    boardOffsetY = (screenHeight - chessBoardSize) / 2;
    srand(time(NULL));
    Image chessImage = LoadImage("images/chess.png");
    SetWindowIcon(chessImage);
    UnloadImage(chessImage);
    LoadChessPieces();
    SetupBoardFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    SetTargetFPS(60);
    
    while (!WindowShouldClose()) {
        UpdateDragAndDrop();

        BeginDrawing();

        ClearBackground(DGRAY);

        DrawChessBoard();
        DrawPieces();

        if (!isWhiteTurn) {
            // Move generation and handling for black pieces should only happen when it's black's turn
            int totalMoveCount;
            Vector2 *moves = GenerateMoves(false, &totalMoveCount); // Generate moves for black pieces
            bool moveMade = false;

            if (totalMoveCount > 0) {
                // Select a random move from the available legal moves
                int randomMoveIndex = GetRandomNumber(0, totalMoveCount - 1);
                Vector2 selectedMove = moves[randomMoveIndex];

                int startX = -1, startY = -1;
                // Find the piece that corresponds to the selected move
                for (int y = 0; y < 8; y++) {
                    for (int x = 0; x < 8; x++) {
                        if (islower(board[y][x])) {
                            int pieceMoveCount;
                            Vector2 *pieceMoves = GeneratePieceMoves(board[y][x], x, y, &pieceMoveCount);
                            for (int j = 0; j < pieceMoveCount; j++) {
                                if (pieceMoves[j].x == selectedMove.x && pieceMoves[j].y == selectedMove.y) {
                                    startX = x;
                                    startY = y;
                                    goto found;
                                }
                            }
                            free(pieceMoves);
                        }
                    }
                }
                found:
                if (startX != -1 && IsValidMove(startX, startY, (int)selectedMove.x, (int)selectedMove.y)) {
                    // Execute the move
                    board[(int)selectedMove.y][(int)selectedMove.x] = board[startY][startX];
                    board[startY][startX] = ' ';
                    isWhiteTurn = true; // Switch turns
                    moveMade = true;
                }
            }
            if (!moveMade) {
                // Handle the situation when no valid moves are available (checkmate or stalemate)
            }

            free(moves);
        }
        // Draw Restart Button
        if (GuiButton((Rectangle){10, screenHeight - 160, 100, 30}, "Restart")) {
            // Reset the game state
            SetupBoardFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            isWhiteTurn = true;
            whiteKingMoved = blackKingMoved = false;
            for (int i = 0; i < 2; i++) {
                whiteRookMoved[i] = blackRookMoved[i] = false;
            }
        }

        // Draw Quit Button
        if (GuiButton((Rectangle){10, screenHeight - 100, 100, 30}, "Quit")) {
            // Close the game window
            CloseWindow();
            return 0; // Exit the program
        }

        EndDrawing();

        // Check for game end conditions
        bool whiteKingInCheck = IsKingInCheck(true);
        bool blackKingInCheck = IsKingInCheck(false);

        int totalMoveCount;
        GenerateMoves(isWhiteTurn, &totalMoveCount);

        if (totalMoveCount == 0) {
            if ((isWhiteTurn && whiteKingInCheck) || (!isWhiteTurn && blackKingInCheck)) {
                printf("%s won by checkmate!\n", isWhiteTurn ? "Black" : "White");
            } else {
                printf("Stalemate!\n");
            }
            break; // Exit the loop to close the game
        }
    }

    CloseWindow();

    return 0;
}