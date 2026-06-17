#include "games.h"

GamesUI::GamesUI(DisplayWrapper* disp, SettingsManager* sm) : display(disp), settings(sm) {
  currentPage = GamesPage::Menu;
  menuSelectedIndex = 0;
  snakeGameOver = false;
  snakeScore = 0;
  tetrisGame = new TetrisGame(disp);
  breakoutGame = new BreakoutGame(disp);
  flappyGame = new FlappyGame(disp);
}

void GamesUI::enter() {
  currentPage = GamesPage::Menu;
  menuSelectedIndex = 0;
  drawMenu();
}

bool GamesUI::update(ButtonEvent btn) {
  if (currentPage == GamesPage::Menu) {
    bool redraw = false;
    if (btn == BTN_DOWN) {
      if (menuSelectedIndex < GAMES_MENU_COUNT - 1) {
        menuSelectedIndex++;
        redraw = true;
      }
    } else if (btn == BTN_UP) {
      if (menuSelectedIndex > 0) {
        menuSelectedIndex--;
        redraw = true;
      }
    } else if (btn == BTN_SELECT) {
      if (menuSelectedIndex == 0) {
        currentPage = GamesPage::Snake;
        initSnake();
      } else if (menuSelectedIndex == 1) {
        currentPage = GamesPage::Pong;
        initPong();
      } else if (menuSelectedIndex == 2) {
        currentPage = GamesPage::Tetris;
        tetrisGame->init();
      } else if (menuSelectedIndex == 3) {
        currentPage = GamesPage::Breakout;
        breakoutGame->init();
      } else if (menuSelectedIndex == 4) {
        currentPage = GamesPage::Flappy;
        flappyGame->init();
      }
      return true;
    } else if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
      return false;
    }
    
    if (redraw) drawMenu();
    return true;
  }
  
  if (currentPage == GamesPage::Snake) {
    handleSnake(btn);
  } else if (currentPage == GamesPage::Pong) {
    handlePong(btn);
  } else if (currentPage == GamesPage::Tetris) {
    if (!tetrisGame->update(btn)) {
      currentPage = GamesPage::Menu;
      drawMenu();
    }
  } else if (currentPage == GamesPage::Breakout) {
    if (!breakoutGame->update(btn)) {
      currentPage = GamesPage::Menu;
      drawMenu();
    }
  } else if (currentPage == GamesPage::Flappy) {
    if (!flappyGame->update(btn)) {
      currentPage = GamesPage::Menu;
      drawMenu();
    }
  }
  
  return true;
}

void GamesUI::drawMenu() {
  display->drawMenu("Games", gamesMenuItems, GAMES_MENU_COUNT, menuSelectedIndex, 0);
}

// ======================= SNAKE =======================

void GamesUI::initSnake() {
  snakeGameOver = false;
  snakeScore = 0;
  snakeLen = 3;
  snakeDir = 1; // Right
  
  snakeX[0] = 64; snakeY[0] = 32;
  snakeX[1] = 60; snakeY[1] = 32;
  snakeX[2] = 56; snakeY[2] = 32;
  
  foodX = random(1, 30) * 4;
  foodY = random(3, 14) * 4;
  lastSnakeMove = millis();
}

void GamesUI::handleSnake(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = GamesPage::Menu;
    drawMenu();
    return;
  }
  
  if (snakeGameOver) {
    if (btn == BTN_SELECT) {
      initSnake();
    }
    return;
  }
  
  if (btn == BTN_UP) {
    snakeDir = (snakeDir + 3) % 4; // Turn Left relatively
  } else if (btn == BTN_DOWN) {
    snakeDir = (snakeDir + 1) % 4; // Turn Right relatively
  }
  
  int speed = 150;
  if (settings->anim_speed == 0) speed = 200;
  if (settings->anim_speed == 2) speed = 80;
  
  if (millis() - lastSnakeMove > speed) {
    lastSnakeMove = millis();
    
    // Move body
    for (int i = snakeLen - 1; i > 0; i--) {
      snakeX[i] = snakeX[i-1];
      snakeY[i] = snakeY[i-1];
    }
    
    // Move head
    if (snakeDir == 0) snakeY[0] -= 4; // Up
    if (snakeDir == 1) snakeX[0] += 4; // Right
    if (snakeDir == 2) snakeY[0] += 4; // Down
    if (snakeDir == 3) snakeX[0] -= 4; // Left
    
    // Check walls
    if (snakeX[0] >= 128 || snakeX[0] < 0 || snakeY[0] < 8 || snakeY[0] >= 64) {
      snakeGameOver = true;
    }
    
    // Check self
    for (int i = 1; i < snakeLen; i++) {
      if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
        snakeGameOver = true;
      }
    }
    
    // Check food
    if (snakeX[0] == foodX && snakeY[0] == foodY) {
      snakeScore++;
      if (snakeLen < 60) snakeLen++;
      foodX = random(1, 31) * 4;
      foodY = random(3, 15) * 4;
    }
    
    // Draw
    display->clear();
    auto d = display->getDriver();
    d->drawLine(0, 8, 128, 8, SSD1306_WHITE);
    d->setCursor(2, 0);
    d->printf("Score: %d", snakeScore);
    
    if (snakeGameOver) {
      d->setCursor(30, 30);
      d->print("GAME OVER");
      d->setCursor(15, 45);
      d->print("SELECT to retry");
    } else {
      d->fillRect(foodX, foodY, 4, 4, SSD1306_WHITE);
      for (int i = 0; i < snakeLen; i++) {
        d->fillRect(snakeX[i], snakeY[i], 4, 4, SSD1306_WHITE);
      }
    }
    d->display();
  }
}

// ======================= PONG =======================

void GamesUI::initPong() {
  pongGameOver = false;
  pongPaused = false;
  pongScore = 0;
  paddleY = 28;
  ballX = 64;
  ballY = 32;
  
  float speed = 1.5f;
  if (settings->anim_speed == 0) speed = 1.0f;
  if (settings->anim_speed == 2) speed = 2.5f;
  
  ballVX = (random(0, 2) == 0 ? -speed : speed);
  ballVY = (random(0, 2) == 0 ? -speed : speed);
  lastPongMove = millis();
}

void GamesUI::handlePong(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    currentPage = GamesPage::Menu;
    drawMenu();
    return;
  }
  
  if (pongGameOver) {
    if (btn == BTN_SELECT || btn == BTN_SELECT_LONG) {
      initPong();
      return;
    }
    
    // Redraw Game Over at a lower rate to prevent I2C flooding
    if (lastPongMove == 0 || millis() - lastPongMove > 250) {
      lastPongMove = millis();
      
      auto d = display->getDriver();
      d->clearDisplay();
      d->setTextSize(1);
      d->setTextColor(SSD1306_WHITE);
      
      d->setCursor(34, 6);
      d->print("GAME OVER");
      
      d->setCursor(8, 22);
      d->print("Score: ");
      d->print(pongScore);
      
      d->setCursor(4, 40);
      d->print("SEL Restart");
      
      d->setCursor(4, 52);
      d->print("BACK Exit");
      
      d->display();
    }
    return;
  }
  
  if (btn == BTN_SELECT_LONG) {
    initPong();
    return;
  }
  
  if (btn == BTN_SELECT) {
    pongPaused = !pongPaused;
  }
  
  // Paddle movement
  if (btn == BTN_UP) {
    paddleY -= 8;
  } else if (btn == BTN_DOWN) {
    paddleY += 8;
  }
  
  if (paddleY < 8) paddleY = 8;
  if (paddleY > 64 - 16) paddleY = 64 - 16;
  
  if (millis() - lastPongMove > 20) {
    lastPongMove = millis();
    
    if (!pongPaused) {
      ballX += ballVX;
      ballY += ballVY;
      
      // Wall bounces
      if (ballY <= 8) { ballY = 8; ballVY = -ballVY; }
      if (ballY >= 60) { ballY = 60; ballVY = -ballVY; }
      if (ballX >= 120) { ballX = 120; ballVX = -ballVX; }
      
      // Player paddle collision
      if (ballX <= 12) {
        if (ballY >= paddleY - 2 && ballY <= paddleY + 16) {
          ballX = 12;
          ballVX = -ballVX;
          pongScore++;
          if (ballVX > 0) ballVX += 0.1f; else ballVX -= 0.1f;
        } else if (ballX <= 0) {
          pongGameOver = true;
          lastPongMove = 0; // Force immediate Game Over redraw
        }
      }
    }
    
    // Draw
    display->clear();
    auto d = display->getDriver();
    d->setTextSize(1);
    d->setTextColor(SSD1306_WHITE);
    d->drawLine(0, 8, 128, 8, SSD1306_WHITE);
    d->setCursor(2, 0);
    d->printf("Score: %d", pongScore);
    
    // Player paddle
    d->fillRect(8, paddleY, 4, 16, SSD1306_WHITE);
    // AI paddle
    int aiY = ballY - 8;
    if (aiY < 8) aiY = 8;
    if (aiY > 64 - 16) aiY = 64 - 16;
    d->fillRect(124, aiY, 4, 16, SSD1306_WHITE);
    // Ball
    d->fillRect((int)ballX, (int)ballY, 4, 4, SSD1306_WHITE);
    
    if (pongPaused) {
      d->fillRect(30, 25, 68, 14, SSD1306_BLACK);
      d->drawRect(30, 25, 68, 14, SSD1306_WHITE);
      d->setCursor(45, 28);
      d->print("PAUSED");
    }
    
    d->display();
  }
}
