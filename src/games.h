#ifndef GAMES_H
#define GAMES_H

#include "app_types.h"
#include "display.h"
#include "buttons.h"
#include "settings.h"

enum class GamesPage {
  Menu,
  Snake,
  Pong
};

class GamesUI {
public:
  GamesUI(DisplayWrapper* disp, SettingsManager* sm);
  void enter();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  SettingsManager* settings;
  GamesPage currentPage;
  
  int menuSelectedIndex;
  
  static const int GAMES_MENU_COUNT = 2;
  const char* gamesMenuItems[2] = {
    "Snake",
    "Pong"
  };

  void drawMenu();
  
  // Snake
  void initSnake();
  void handleSnake(ButtonEvent btn);
  bool snakeGameOver;
  int snakeScore;
  int snakeDir; // 0=U, 1=R, 2=D, 3=L
  int snakeLen;
  int16_t snakeX[60];
  int16_t snakeY[60];
  int16_t foodX, foodY;
  unsigned long lastSnakeMove;

  // Pong
  void initPong();
  void handlePong(ButtonEvent btn);
  bool pongGameOver;
  bool pongPaused;
  int pongScore;
  int16_t paddleY;
  float ballX, ballY;
  float ballVX, ballVY;
  unsigned long lastPongMove;
};

#endif
