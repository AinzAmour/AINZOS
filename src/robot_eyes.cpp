#include "robot_eyes.h"
#include <time.h>
#include <Arduino.h>

// Current state of the eyes
static EyeState left_eye, right_eye;
static int current_corner_radius = REF_CORNER_RADIUS;

// Animation state machine variables
static EyeAnimState animState = ANIM_IDLE;
static uint32_t lastAnimTick = 0;
static int animProgress = 0; // Generic counter for loops

// Helper for GFX primitives
static int calculate_safe_radius(int r, int w, int h) {
  if (w < 2 * (r + 1)) r = (w / 2) - 1;
  if (h < 2 * (r + 1)) r = (h / 2) - 1;
  return (r < 0) ? 0 : r;
}

static void snapToCenter() {
  left_eye.height = REF_EYE_HEIGHT;
  left_eye.width = REF_EYE_WIDTH;
  right_eye.height = REF_EYE_HEIGHT;
  right_eye.width = REF_EYE_WIDTH;
  
  left_eye.x = 64 - REF_EYE_WIDTH / 2 - REF_SPACE_BETWEEN_EYE / 2;
  left_eye.y = 32;
  right_eye.x = 64 + REF_EYE_WIDTH / 2 + REF_SPACE_BETWEEN_EYE / 2;
  right_eye.y = 32;

  current_corner_radius = REF_CORNER_RADIUS;
}

void initRobotEyes() {
  snapToCenter();
  animState = ANIM_IDLE;
  lastAnimTick = 0;
  animProgress = 0;
}

// Map the target Mood to an animation transition if we are idle
static void handleMoodTransitions(RobotMood targetMood) {
  if (animState != ANIM_IDLE) return; // Wait for current animation to finish

  switch(targetMood) {
    case MOOD_BLINK:
      animState = ANIM_BLINK_CLOSING;
      animProgress = 0;
      break;
    case MOOD_LOOK_LEFT:
      animState = ANIM_LOOKING_LEFT;
      animProgress = 0;
      break;
    case MOOD_LOOK_RIGHT:
      animState = ANIM_LOOKING_RIGHT;
      animProgress = 0;
      break;
    case MOOD_HAPPY:
      animState = ANIM_HAPPY_TRANSITION;
      animProgress = 0;
      break;
    case MOOD_SLEEPY:
      animState = ANIM_SLEEPING_TRANSITION;
      animProgress = 0;
      break;
    case MOOD_SURPRISED:
      animState = ANIM_SURPRISE_TRANSITION;
      animProgress = 0;
      break;
    case MOOD_IDLE:
    default:
      if (left_eye.x != 64 - REF_EYE_WIDTH / 2 - REF_SPACE_BETWEEN_EYE / 2 || 
          left_eye.height != REF_EYE_HEIGHT) {
        animState = ANIM_RETURN_TO_CENTER;
        animProgress = 0;
      }
      break;
  }
}

// Tick the animation state machine
static void tickAnimationMachine(uint32_t now) {
  // Use a fixed 50ms per frame for smooth 20 FPS motion
  if (now - lastAnimTick < 50) return;
  lastAnimTick = now;

  int speed = 8; // Pixels per frame

  switch(animState) {
    case ANIM_IDLE:
      // Do nothing, handled by mood transitions
      break;
      
    case ANIM_BLINK_CLOSING:
      left_eye.height -= speed;
      right_eye.height -= speed;
      left_eye.width += 2;
      right_eye.width += 2;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 4) {
        animState = ANIM_BLINK_OPENING;
        animProgress = 0;
      }
      break;
      
    case ANIM_BLINK_OPENING:
      left_eye.height += speed;
      right_eye.height += speed;
      left_eye.width -= 2;
      right_eye.width -= 2;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 4) {
        snapToCenter();
        animState = ANIM_IDLE;
      }
      break;

    case ANIM_RETURN_TO_CENTER:
      // Linear interpolation back to idle
      snapToCenter();
      animState = ANIM_IDLE;
      break;

    case ANIM_LOOKING_LEFT:
      left_eye.x -= 4;
      right_eye.x -= 4;
      animProgress++;
      if (animProgress >= 3) animState = ANIM_IDLE;
      break;

    case ANIM_LOOKING_RIGHT:
      left_eye.x += 4;
      right_eye.x += 4;
      animProgress++;
      if (animProgress >= 3) animState = ANIM_IDLE;
      break;

    case ANIM_HAPPY_TRANSITION:
      // Just hold, render pass will handle the happy cutouts
      animProgress++;
      if (animProgress >= 5) animState = ANIM_HAPPY_HOLD;
      break;

    case ANIM_HAPPY_HOLD:
      // Wait for mood to change
      break;

    case ANIM_SLEEPING_TRANSITION:
      left_eye.height -= 4;
      right_eye.height -= 4;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 8) animState = ANIM_SLEEPING_HOLD;
      break;

    case ANIM_SLEEPING_HOLD:
      break;

    case ANIM_SURPRISE_TRANSITION:
      left_eye.height -= 4;
      right_eye.height -= 4;
      left_eye.width -= 4;
      right_eye.width -= 4;
      animProgress++;
      if (animProgress >= 4) animState = ANIM_SURPRISE_HOLD;
      break;

    case ANIM_SURPRISE_HOLD:
      break;
  }
}

void updateRobotEyes(DisplayWrapper* display, SettingsManager* settings, RobotMood targetMood, uint32_t now) {
  handleMoodTransitions(targetMood);
  tickAnimationMachine(now);

  auto d = display->getDriver();
  display->clear();

  // Draw the actual eyes based on calculated EyeState dimensions
  int r_left = calculate_safe_radius(current_corner_radius, left_eye.width, left_eye.height);
  int x_left = left_eye.x - left_eye.width / 2;
  int y_left = left_eye.y - left_eye.height / 2;
  d->fillRoundRect(x_left, y_left, left_eye.width, left_eye.height, r_left, SSD1306_WHITE);

  int r_right = calculate_safe_radius(current_corner_radius, right_eye.width, right_eye.height);
  int x_right = right_eye.x - right_eye.width / 2;
  int y_right = right_eye.y - right_eye.height / 2;
  d->fillRoundRect(x_right, y_right, right_eye.width, right_eye.height, r_right, SSD1306_WHITE);

  // If happy, overlay a black triangle on bottom half to simulate a cheek raise smile
  if (animState == ANIM_HAPPY_TRANSITION || animState == ANIM_HAPPY_HOLD) {
    int offset = 10;
    d->fillTriangle(x_left - 1, y_left + offset, x_left + left_eye.width + 1, y_left + 5 + offset, x_left - 1, y_left + left_eye.height + offset, SSD1306_BLACK);
    d->fillTriangle(x_right + right_eye.width + 1, y_right + offset, x_right - 2, y_right + 5 + offset, x_right + right_eye.width + 1, y_right + right_eye.height + offset, SSD1306_BLACK);
  }

  // Overlay small time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    char timeStr[10];
    int h = timeinfo.tm_hour;
    if (!settings->cs.use24h) {
      h = h % 12;
      if (h == 0) h = 12;
    }
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, timeinfo.tm_min);
    d->setTextSize(1);
    d->setTextColor(SSD1306_WHITE);
    // Draw at bottom center
    d->setCursor(48, 54);
    d->print(timeStr);
  }

  d->display();
}
