#include "breakout.h"

BreakoutGame::BreakoutGame(DisplayWrapper* disp) : display(disp) {
  gameOver = true;
  paused = false;
  score = 0;
  lives = 3;
  level = 1;
  highScore = 0;
}

void BreakoutGame::loadHighScore() {
  Preferences prefs;
  prefs.begin("ainzos", true);
  highScore = prefs.getInt("breakout_hs", 0);
  prefs.end();
}

void BreakoutGame::saveHighScore() {
  if (score > highScore) {
    highScore = score;
    Preferences prefs;
    prefs.begin("ainzos", false);
    prefs.putInt("breakout_hs", highScore);
    prefs.end();
  }
}

void BreakoutGame::initBricks() {
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      // Top row = strong bricks (2 hits), rest = normal (1 hit)
      if (r == 0 && level > 1) {
        bricks[r][c] = 2;  // Strong bricks in higher levels
      } else {
        bricks[r][c] = 1;
      }
    }
  }
  bricksRemaining = countBricks();
}

int BreakoutGame::countBricks() {
  int count = 0;
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      if (bricks[r][c] > 0) count++;
    }
  }
  return count;
}

void BreakoutGame::resetBall() {
  ballX = (float)paddleX + PADDLE_W / 2.0f;
  ballY = PADDLE_Y - 3.0f;
  ballLaunched = false;
  // Random upward angle
  float angle = -0.7f - (float)(random(0, 60)) / 100.0f; // -0.7 to -1.3 rad
  float speed = 1.5f + (float)level * 0.1f;
  ballVX = speed * cos(angle) * (random(0, 2) == 0 ? -1.0f : 1.0f);
  ballVY = -speed;
}

void BreakoutGame::init() {
  score = 0;
  lives = 3;
  level = 1;
  gameOver = false;
  paused = false;
  paddleX = 52;  // Center paddle
  loadHighScore();
  initBricks();
  resetBall();
  lastFrameMs = millis();
}

bool BreakoutGame::update(ButtonEvent btn) {
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
    drawFrame();
    return true;
  }

  // Controls: UP = move left, DOWN = move right
  if (btn == BTN_UP) {
    paddleX -= 8;
    if (paddleX < 0) paddleX = 0;
  } else if (btn == BTN_DOWN) {
    paddleX += 8;
    if (paddleX > 128 - PADDLE_W) paddleX = 128 - PADDLE_W;
  }

  // Launch ball
  if (!ballLaunched) {
    if (btn == BTN_SELECT) {
      ballLaunched = true;
    }
    ballX = (float)paddleX + PADDLE_W / 2.0f;
    ballY = PADDLE_Y - 3.0f;
  }

  // Physics at ~30fps
  if (millis() - lastFrameMs >= 33) {
    lastFrameMs = millis();

    if (ballLaunched) {
      ballX += ballVX;
      ballY += ballVY;

      // Wall bounces
      if (ballX <= 0) { ballX = 0; ballVX = -ballVX; }
      if (ballX >= 126) { ballX = 126; ballVX = -ballVX; }
      if (ballY <= 0) { ballY = 0; ballVY = -ballVY; }

      // Paddle collision
      if (ballY >= PADDLE_Y - 2 && ballY <= PADDLE_Y &&
          ballX >= paddleX - 2 && ballX <= paddleX + PADDLE_W) {
        ballVY = -fabs(ballVY);
        // Adjust angle based on where ball hits paddle
        float hitPos = (ballX - (float)paddleX) / (float)PADDLE_W; // 0..1
        ballVX = (hitPos - 0.5f) * 3.0f;
        ballY = PADDLE_Y - 3.0f;
      }

      // Ball missed paddle
      if (ballY > 63) {
        lives--;
        if (lives <= 0) {
          gameOver = true;
          saveHighScore();
        } else {
          resetBall();
        }
      }

      // Brick collision
      for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
          if (bricks[r][c] == 0) continue;

          int bx = c * (BRICK_W + BRICK_GAP) + 4;
          int by = r * (BRICK_H + BRICK_GAP) + 2;

          // AABB check
          if (ballX + 2 >= bx && ballX <= bx + BRICK_W &&
              ballY + 2 >= by && ballY <= by + BRICK_H) {
            // Determine which face was hit
            float overlapLeft = (ballX + 2) - bx;
            float overlapRight = (bx + BRICK_W) - ballX;
            float overlapTop = (ballY + 2) - by;
            float overlapBottom = (by + BRICK_H) - ballY;

            float minOverlapX = (overlapLeft < overlapRight) ? overlapLeft : overlapRight;
            float minOverlapY = (overlapTop < overlapBottom) ? overlapTop : overlapBottom;

            if (minOverlapX < minOverlapY) {
              ballVX = -ballVX;
            } else {
              ballVY = -ballVY;
            }

            bricks[r][c]--;
            if (bricks[r][c] == 0) {
              score += 10 * level;
              bricksRemaining--;
            }

            // Only handle one brick collision per frame
            goto brick_done;
          }
        }
      }
      brick_done:

      // Level clear
      if (bricksRemaining <= 0) {
        level++;
        initBricks();
        resetBall();
      }
    }

    drawFrame();
  }

  return true;
}

void BreakoutGame::drawFrame() {
  display->clear();
  auto d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);

  // Draw bricks
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      if (bricks[r][c] == 0) continue;
      int bx = c * (BRICK_W + BRICK_GAP) + 4;
      int by = r * (BRICK_H + BRICK_GAP) + 2;
      if (bricks[r][c] == 2) {
        // Strong brick — border only (darker)
        d->drawRect(bx, by, BRICK_W, BRICK_H, SSD1306_WHITE);
      } else {
        // Normal brick — filled
        d->fillRect(bx, by, BRICK_W, BRICK_H, SSD1306_WHITE);
      }
    }
  }

  // Draw paddle
  d->fillRect(paddleX, PADDLE_Y, PADDLE_W, PADDLE_H, SSD1306_WHITE);

  // Draw ball
  d->fillRect((int)ballX, (int)ballY, 2, 2, SSD1306_WHITE);

  // HUD: lives and score at bottom-right area, tiny text
  // Use top-right corner for score
  char buf[16];
  snprintf(buf, sizeof(buf), "S:%d L:%d", score, lives);
  d->setCursor(70, 56);
  d->print(buf);

  if (paused) {
    d->fillRect(36, 26, 56, 12, SSD1306_BLACK);
    d->drawRect(36, 26, 56, 12, SSD1306_WHITE);
    d->setCursor(44, 28);
    d->print(F("PAUSED"));
  }

  if (!ballLaunched && !gameOver) {
    d->setCursor(20, 36);
    d->print(F("SEL to launch"));
  }

  d->display();
}

void BreakoutGame::drawGameOver() {
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
