#ifndef TETRIS_H
#define TETRIS_H

#include "display.h"
#include "buttons.h"
#include <Preferences.h>

// 7 Tetrominoes, 4 rotations each, stored as 4x4 bitmask (16 bits)
static const uint16_t TETROMINOS[7][4] PROGMEM = {
  // I
  { 0x0F00, 0x2222, 0x00F0, 0x4444 },
  // O
  { 0x6600, 0x6600, 0x6600, 0x6600 },
  // T
  { 0x0E40, 0x4C40, 0x4E00, 0x4640 },
  // S
  { 0x06C0, 0x8C40, 0x6C00, 0x4620 },
  // Z
  { 0x0C60, 0x4C80, 0xC600, 0x2640 },
  // J
  { 0x0E80, 0xC440, 0x2E00, 0x44C0 },
  // L
  { 0x0E20, 0x44C0, 0x8E00, 0xC440 }
};

class TetrisGame {
public:
  TetrisGame(DisplayWrapper* disp);
  void init();
  bool update(ButtonEvent btn);  // returns false to exit

private:
  DisplayWrapper* display;

  // Board: 10 wide x 20 tall
  static const int BOARD_W = 10;
  static const int BOARD_H = 20;
  static const int CELL = 3;  // 3px per cell to fit 30x60 in display
  uint8_t board[BOARD_H][BOARD_W];  // 0=empty, 1=filled

  // Current piece
  int pieceType;
  int pieceRot;
  int pieceX, pieceY;

  // Next piece
  int nextType;

  // Game state
  int score;
  int level;
  int linesCleared;
  int highScore;
  bool gameOver;
  bool paused;

  unsigned long lastDropMs;
  unsigned long lockDelayMs;
  bool lockStarted;
  unsigned long lockStartMs;

  void spawnPiece();
  bool checkCollision(int type, int rot, int x, int y);
  void lockPiece();
  void clearLines();
  int getDropInterval();
  void rotatePiece();
  void drawBoard();
  void drawPiece(int type, int rot, int x, int y, int offX, int offY);
  void drawSidePanel();
  void drawGameOver();
  void loadHighScore();
  void saveHighScore();

  bool getPieceCell(int type, int rot, int cx, int cy);
};

#endif
