#ifndef BREAKOUT_H
#define BREAKOUT_H

#include "display.h"
#include "buttons.h"
#include <Preferences.h>

class BreakoutGame {
public:
  BreakoutGame(DisplayWrapper* disp);
  void init();
  bool update(ButtonEvent btn);  // returns false to exit

private:
  DisplayWrapper* display;

  // Brick grid: 8 cols x 5 rows
  static const int BRICK_COLS = 8;
  static const int BRICK_ROWS = 5;
  static const int BRICK_W = 14;
  static const int BRICK_H = 5;
  static const int BRICK_GAP = 1;
  uint8_t bricks[BRICK_ROWS][BRICK_COLS];  // 0=destroyed, 1=normal, 2=strong(2hits)

  // Paddle
  int paddleX;
  static const int PADDLE_W = 24;
  static const int PADDLE_H = 3;
  static const int PADDLE_Y = 60;

  // Ball
  float ballX, ballY;
  float ballVX, ballVY;
  bool ballLaunched;

  // Game state
  int score;
  int lives;
  int level;
  int highScore;
  int bricksRemaining;
  bool gameOver;
  bool paused;
  unsigned long lastFrameMs;

  void resetBall();
  void initBricks();
  int countBricks();
  void drawFrame();
  void drawGameOver();
  void loadHighScore();
  void saveHighScore();
};

#endif
