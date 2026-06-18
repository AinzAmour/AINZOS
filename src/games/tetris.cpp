#include "tetris.h"

TetrisGame::TetrisGame(DisplayWrapper* disp) : display(disp) {
  gameOver = true;
  paused = false;
  score = 0;
  level = 1;
  linesCleared = 0;
  highScore = 0;
}

bool TetrisGame::getPieceCell(int type, int rot, int cx, int cy) {
  uint16_t mask = pgm_read_word(&TETROMINOS[type][rot]);
  int bit = cy * 4 + cx;
  return (mask >> (15 - bit)) & 1;
}

void TetrisGame::loadHighScore() {
  Preferences prefs;
  prefs.begin("ainzos", true);
  highScore = prefs.getInt("tetris_hs", 0);
  prefs.end();
}

void TetrisGame::saveHighScore() {
  if (score > highScore) {
    highScore = score;
    Preferences prefs;
    prefs.begin("ainzos", false);
    prefs.putInt("tetris_hs", highScore);
    prefs.end();
  }
}

void TetrisGame::init() {
  memset(board, 0, sizeof(board));
  score = 0;
  level = 1;
  linesCleared = 0;
  gameOver = false;
  paused = false;
  loadHighScore();
  nextType = random(0, 7);
  spawnPiece();
  lastDropMs = millis();
  lockStarted = false;
}

void TetrisGame::spawnPiece() {
  pieceType = nextType;
  nextType = random(0, 7);
  pieceRot = 0;
  pieceX = 3;
  pieceY = 0;
  lockStarted = false;

  if (checkCollision(pieceType, pieceRot, pieceX, pieceY)) {
    gameOver = true;
    saveHighScore();
  }
}

bool TetrisGame::checkCollision(int type, int rot, int x, int y) {
  for (int cy = 0; cy < 4; cy++) {
    for (int cx = 0; cx < 4; cx++) {
      if (!getPieceCell(type, rot, cx, cy)) continue;
      int bx = x + cx;
      int by = y + cy;
      if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
      if (by >= 0 && board[by][bx]) return true;
    }
  }
  return false;
}

void TetrisGame::lockPiece() {
  for (int cy = 0; cy < 4; cy++) {
    for (int cx = 0; cx < 4; cx++) {
      if (!getPieceCell(pieceType, pieceRot, cx, cy)) continue;
      int bx = pieceX + cx;
      int by = pieceY + cy;
      if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
        board[by][bx] = 1;
      }
    }
  }
  clearLines();
  spawnPiece();
}

void TetrisGame::clearLines() {
  int cleared = 0;
  for (int y = BOARD_H - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < BOARD_W; x++) {
      if (!board[y][x]) { full = false; break; }
    }
    if (full) {
      cleared++;
      // Shift everything down
      for (int yy = y; yy > 0; yy--) {
        memcpy(board[yy], board[yy - 1], BOARD_W);
      }
      memset(board[0], 0, BOARD_W);
      y++; // Re-check same row
    }
  }

  if (cleared > 0) {
    const int lineScores[] = { 0, 100, 300, 500, 800 };
    int idx = (cleared > 4) ? 4 : cleared;
    score += lineScores[idx] * level;
    linesCleared += cleared;
    level = 1 + linesCleared / 10;
  }
}

int TetrisGame::getDropInterval() {
  int interval = 500 - (level - 1) * 40;
  if (interval < 100) interval = 100;
  return interval;
}

void TetrisGame::rotatePiece() {
  int newRot = (pieceRot + 1) % 4;
  if (!checkCollision(pieceType, newRot, pieceX, pieceY)) {
    pieceRot = newRot;
  }
  // Wall kick: try shifting left or right
  else if (!checkCollision(pieceType, newRot, pieceX - 1, pieceY)) {
    pieceX--;
    pieceRot = newRot;
  }
  else if (!checkCollision(pieceType, newRot, pieceX + 1, pieceY)) {
    pieceX++;
    pieceRot = newRot;
  }
}

bool TetrisGame::update(ButtonEvent btn) {
  if (btn == BTN_BACK_LONG) {
    saveHighScore();
    return false;
  }

  if (gameOver) {
    drawGameOver();
    if (btn == BTN_SELECT || btn == BTN_SELECT_LONG) {
      init();
    }
    return true;
  }

  if (btn == BTN_SELECT_LONG) {
    paused = !paused;
  }

  if (paused) {
    drawBoard();
    return true;
  }

  // Controls
  if (btn == BTN_UP) {
    rotatePiece();
  } else if (btn == BTN_DOWN) {
    // Soft drop
    if (!checkCollision(pieceType, pieceRot, pieceX, pieceY + 1)) {
      pieceY++;
      lastDropMs = millis();
    }
  } else if (btn == BTN_DOWN_LONG) {
    // Hard drop
    while (!checkCollision(pieceType, pieceRot, pieceX, pieceY + 1)) {
      pieceY++;
    }
    lockPiece();
    lastDropMs = millis();
  } else if (btn == BTN_SELECT) {
    // Move right
    if (!checkCollision(pieceType, pieceRot, pieceX + 1, pieceY)) {
      pieceX++;
    }
  } else if (btn == BTN_BACK) {
    // Move left
    if (!checkCollision(pieceType, pieceRot, pieceX - 1, pieceY)) {
      pieceX--;
    }
  }

  // Gravity
  if (millis() - lastDropMs >= (unsigned long)getDropInterval()) {
    lastDropMs = millis();
    if (!checkCollision(pieceType, pieceRot, pieceX, pieceY + 1)) {
      pieceY++;
      lockStarted = false;
    } else {
      // Piece can't move down — start lock delay
      if (!lockStarted) {
        lockStarted = true;
        lockStartMs = millis();
      }
    }
  }

  // Lock delay
  if (lockStarted && millis() - lockStartMs >= 300) {
    if (checkCollision(pieceType, pieceRot, pieceX, pieceY + 1)) {
      lockPiece();
    }
    lockStarted = false;
  }

  drawBoard();
  return true;
}

void TetrisGame::drawBoard() {
  display->clear();
  auto d = display->getDriver();

  // Board area: 10 cols * 3px = 30px wide, 20 rows * 3px = 60px tall
  // Start at (1, 2) to leave room for border
  int offX = 1;
  int offY = 2;

  // Draw board border
  d->drawRect(0, 1, 32, 62, SSD1306_WHITE);

  // Draw locked cells
  for (int y = 0; y < BOARD_H; y++) {
    for (int x = 0; x < BOARD_W; x++) {
      if (board[y][x]) {
        d->fillRect(offX + x * CELL, offY + y * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
      }
    }
  }

  // Draw current piece
  if (!gameOver) {
    drawPiece(pieceType, pieceRot, pieceX, pieceY, offX, offY);
  }

  // Side panel
  drawSidePanel();

  if (paused) {
    d->fillRect(36, 26, 50, 12, SSD1306_BLACK);
    d->drawRect(36, 26, 50, 12, SSD1306_WHITE);
    d->setCursor(44, 28);
    d->print(F("PAUSED"));
  }

  d->display();
}

void TetrisGame::drawPiece(int type, int rot, int x, int y, int offX, int offY) {
  auto d = display->getDriver();
  for (int cy = 0; cy < 4; cy++) {
    for (int cx = 0; cx < 4; cx++) {
      if (!getPieceCell(type, rot, cx, cy)) continue;
      int bx = x + cx;
      int by = y + cy;
      if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
        d->fillRect(offX + bx * CELL, offY + by * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
      }
    }
  }
}

void TetrisGame::drawSidePanel() {
  auto d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);

  // Score
  d->setCursor(35, 2);
  d->printf("Scr:%d", score);

  // Level
  d->setCursor(35, 12);
  d->printf("Lvl:%d", level);

  // Lines
  d->setCursor(35, 22);
  d->printf("Lns:%d", linesCleared);

  // High Score
  d->setCursor(35, 32);
  d->printf("Hi:%d", highScore);

  // Next piece preview
  d->setCursor(35, 44);
  d->print(F("Next:"));
  // Draw next piece at (35, 52) scaled to small 2px cells
  for (int cy = 0; cy < 4; cy++) {
    for (int cx = 0; cx < 4; cx++) {
      if (getPieceCell(nextType, 0, cx, cy)) {
        d->fillRect(35 + cx * 2, 52 + cy * 2, 2, 2, SSD1306_WHITE);
      }
    }
  }
}

void TetrisGame::drawGameOver() {
  display->clear();
  auto d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);

  d->setCursor(28, 6);
  d->print(F("GAME OVER"));

  d->setCursor(20, 22);
  d->printf("Score: %d", score);

  d->setCursor(20, 34);
  d->printf("High: %d", highScore);

  d->setCursor(10, 50);
  d->print(F("SEL:Retry BACK:Exit"));

  d->display();
}
