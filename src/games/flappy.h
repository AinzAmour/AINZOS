#ifndef FLAPPY_H
#define FLAPPY_H

#include "display.h"
#include "buttons.h"
#include <Preferences.h>

struct Pipe {
  float x;
  int gapY;    // Y position of gap center
  bool scored; // Already passed by bird
};

class FlappyGame {
public:
  FlappyGame(DisplayWrapper* disp);
  void init();
  bool update(ButtonEvent btn);  // returns false to exit

private:
  DisplayWrapper* display;

  // Bird
  float birdY;
  float birdVY;
  static const int BIRD_X = 20;
  static const int BIRD_SIZE = 5;

  // Pipes
  static const int MAX_PIPES = 4;
  static const int PIPE_W = 16;
  static const int PIPE_GAP = 18;
  Pipe pipes[MAX_PIPES];
  int pipeCount;
  float pipeSpeed;
  unsigned long lastPipeSpawnMs;

  // Game state
  int score;
  int highScore;
  bool gameOver;
  bool started;
  unsigned long lastFrameMs;

  void spawnPipe();
  bool checkCollision();
  void drawFrame();
  void drawGameOver();
  void drawBird(Adafruit_SSD1306* d);
  void loadHighScore();
  void saveHighScore();
};

#endif
