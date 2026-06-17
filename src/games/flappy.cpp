#include "flappy.h"

FlappyGame::FlappyGame(DisplayWrapper* disp) : display(disp) {
  gameOver = true;
  started = false;
  score = 0;
  highScore = 0;
}

void FlappyGame::loadHighScore() {
  Preferences prefs;
  prefs.begin("hizmos", true);
  highScore = prefs.getInt("flappy_hs", 0);
  prefs.end();
}

void FlappyGame::saveHighScore() {
  if (score > highScore) {
    highScore = score;
    Preferences prefs;
    prefs.begin("hizmos", false);
    prefs.putInt("flappy_hs", highScore);
    prefs.end();
  }
}

void FlappyGame::init() {
  birdY = 32.0f;
  birdVY = 0.0f;
  score = 0;
  gameOver = false;
  started = false;
  pipeCount = 0;
  pipeSpeed = 2.0f;
  lastFrameMs = millis();
  lastPipeSpawnMs = millis();
  loadHighScore();

  // Clear pipes
  for (int i = 0; i < MAX_PIPES; i++) {
    pipes[i].x = -100;
    pipes[i].scored = true;
  }
}

void FlappyGame::spawnPipe() {
  // Find a free slot
  int slot = -1;
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x < -PIPE_W) {
      slot = i;
      break;
    }
  }
  if (slot < 0) return;

  pipes[slot].x = 128.0f;
  pipes[slot].gapY = 10 + random(0, 36);  // gap center between 10 and 46
  pipes[slot].scored = false;
  pipeCount++;
}

bool FlappyGame::checkCollision() {
  // Floor and ceiling
  if (birdY < 0 || birdY + BIRD_SIZE > 63) return true;

  // Pipe collision (AABB)
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x < -PIPE_W || pipes[i].x > 128) continue;

    int px = (int)pipes[i].x;
    int gapTop = pipes[i].gapY - PIPE_GAP / 2;
    int gapBot = pipes[i].gapY + PIPE_GAP / 2;

    // Check if bird overlaps pipe horizontally
    if (BIRD_X + BIRD_SIZE > px && BIRD_X < px + PIPE_W) {
      // Check if bird is outside the gap
      if ((int)birdY < gapTop || (int)birdY + BIRD_SIZE > gapBot) {
        return true;
      }
    }
  }
  return false;
}

bool FlappyGame::update(ButtonEvent btn) {
  if (btn == BTN_BACK_LONG) {
    saveHighScore();
    return false;
  }

  if (gameOver) {
    drawGameOver();
    if (btn == BTN_SELECT_LONG) {
      init();
    }
    return true;
  }

  // Any button = flap (when playing) or start
  if (btn == BTN_UP || btn == BTN_DOWN || btn == BTN_SELECT || btn == BTN_BACK) {
    if (!started) {
      started = true;
      birdVY = -3.5f;
    } else {
      birdVY = -3.5f;  // Flap
    }
  }

  // Physics at ~30fps
  if (millis() - lastFrameMs >= 33) {
    lastFrameMs = millis();

    if (started) {
      // Gravity
      birdVY += 0.4f;
      birdY += birdVY;

      // Move pipes
      for (int i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].x < -PIPE_W) continue;
        pipes[i].x -= pipeSpeed;

        // Score when bird passes pipe
        if (!pipes[i].scored && pipes[i].x + PIPE_W < BIRD_X) {
          pipes[i].scored = true;
          score++;
          // Increase difficulty every 5 points
          if (score % 5 == 0) {
            pipeSpeed += 0.1f;
          }
        }
      }

      // Spawn pipes periodically
      if (millis() - lastPipeSpawnMs > (unsigned long)(1200 / pipeSpeed * 2)) {
        spawnPipe();
        lastPipeSpawnMs = millis();
      }

      // Collision check
      if (checkCollision()) {
        gameOver = true;
        saveHighScore();
      }
    }

    drawFrame();
  }

  return true;
}

void FlappyGame::drawFrame() {
  display->clear();
  auto d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);

  // Draw ground line
  d->drawFastHLine(0, 63, 128, SSD1306_WHITE);

  // Draw pipes
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x < -PIPE_W || pipes[i].x > 128) continue;

    int px = (int)pipes[i].x;
    int gapTop = pipes[i].gapY - PIPE_GAP / 2;
    int gapBot = pipes[i].gapY + PIPE_GAP / 2;

    // Top pipe
    if (gapTop > 0) {
      d->fillRect(px, 0, PIPE_W, gapTop, SSD1306_WHITE);
      // Pipe lip
      d->fillRect(px - 2, gapTop - 3, PIPE_W + 4, 3, SSD1306_WHITE);
    }

    // Bottom pipe
    if (gapBot < 63) {
      d->fillRect(px, gapBot, PIPE_W, 63 - gapBot, SSD1306_WHITE);
      // Pipe lip
      d->fillRect(px - 2, gapBot, PIPE_W + 4, 3, SSD1306_WHITE);
    }
  }

  // Draw bird
  drawBird(d);

  // Score display
  d->setCursor(2, 2);
  d->printf("Score:%d", score);

  if (!started && !gameOver) {
    d->setCursor(20, 30);
    d->print(F("Press to start!"));
  }

  d->display();
}

void FlappyGame::drawBird(Adafruit_SSD1306* d) {
  int by = (int)birdY;

  // Simple bird sprite: 5x5 with wing
  // Body
  d->fillRect(BIRD_X + 1, by + 1, 3, 3, SSD1306_WHITE);
  // Head
  d->drawPixel(BIRD_X + 4, by + 1, SSD1306_WHITE);
  // Eye
  d->drawPixel(BIRD_X + 3, by + 1, SSD1306_BLACK);
  // Beak
  d->drawPixel(BIRD_X + 4, by + 2, SSD1306_WHITE);
  // Wing (flap animation)
  if ((millis() / 100) % 2 == 0) {
    d->drawPixel(BIRD_X, by, SSD1306_WHITE);
    d->drawPixel(BIRD_X + 1, by, SSD1306_WHITE);
  } else {
    d->drawPixel(BIRD_X, by + 3, SSD1306_WHITE);
    d->drawPixel(BIRD_X + 1, by + 4, SSD1306_WHITE);
  }
  // Tail
  d->drawPixel(BIRD_X, by + 2, SSD1306_WHITE);
}

void FlappyGame::drawGameOver() {
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

  d->setCursor(4, 50);
  d->print(F("SEL+HOLD:Retry BK:Ex"));

  d->display();
}
