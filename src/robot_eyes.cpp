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
  // If we are currently holding a mood, and the target mood has changed, transition to return state
  if (animState == ANIM_BLINK_HOLD && targetMood != MOOD_BLINK) {
    snapToCenter();
    animState = ANIM_IDLE;
    animProgress = 0;
  } else if (animState == ANIM_LOOK_LEFT_HOLD && targetMood != MOOD_LOOK_LEFT) {
    animState = ANIM_LOOK_LEFT_RETURN_1;
    animProgress = 0;
  } else if (animState == ANIM_LOOK_RIGHT_HOLD && targetMood != MOOD_LOOK_RIGHT) {
    animState = ANIM_LOOK_RIGHT_RETURN_1;
    animProgress = 0;
  } else if (animState == ANIM_HAPPY_HOLD && targetMood != MOOD_HAPPY) {
    animState = ANIM_HAPPY_RETURN;
    animProgress = 0;
  } else if (animState == ANIM_SLEEPING_HOLD && targetMood != MOOD_SLEEPY) {
    animState = ANIM_SLEEPING_RETURN;
    animProgress = 0;
  } else if (animState == ANIM_SURPRISE_HOLD && targetMood != MOOD_SURPRISED) {
    animState = ANIM_SURPRISE_RETURN;
    animProgress = 0;
  }

  if (animState != ANIM_IDLE) return; // Wait for current animation to finish

  switch(targetMood) {
    case MOOD_BLINK:
      animState = ANIM_BLINK_CLOSING;
      animProgress = 0;
      break;
    case MOOD_LOOK_LEFT:
      animState = ANIM_LOOK_LEFT_TRANSITION_1;
      animProgress = 0;
      break;
    case MOOD_LOOK_RIGHT:
      animState = ANIM_LOOK_RIGHT_TRANSITION_1;
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
      break;
  }
}

// Tick the animation state machine
static void tickAnimationMachine(uint32_t now) {
  // Use a fixed 50ms per frame for smooth 20 FPS motion
  if (now - lastAnimTick < 50) return;
  lastAnimTick = now;

  switch(animState) {
    case ANIM_IDLE:
      break;
      
    case ANIM_BLINK_CLOSING:
      left_eye.height -= 12;
      right_eye.height -= 12;
      left_eye.width += 3;
      right_eye.width += 3;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_BLINK_OPENING;
        animProgress = 0;
      }
      break;
      
    case ANIM_BLINK_OPENING:
      left_eye.height += 12;
      right_eye.height += 12;
      left_eye.width -= 3;
      right_eye.width -= 3;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 3) {
        snapToCenter();
        animState = ANIM_BLINK_HOLD;
        animProgress = 0;
      }
      break;

    case ANIM_BLINK_HOLD:
      break;

    // LOOK LEFT TRANSITIONS:
    case ANIM_LOOK_LEFT_TRANSITION_1:
      left_eye.x -= 2;
      right_eye.x -= 2;
      left_eye.height -= 5;
      right_eye.height -= 5;
      left_eye.height += 1;
      left_eye.width += 1;
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_LOOK_LEFT_TRANSITION_2;
        animProgress = 0;
      }
      break;

    case ANIM_LOOK_LEFT_TRANSITION_2:
      left_eye.x -= 2;
      right_eye.x -= 2;
      left_eye.height += 5;
      right_eye.height += 5;
      left_eye.height += 1;
      left_eye.width += 1;
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_LOOK_LEFT_HOLD;
        animProgress = 0;
      }
      break;

    case ANIM_LOOK_LEFT_HOLD:
      break;

    case ANIM_LOOK_LEFT_RETURN_1:
      left_eye.x += 2;
      right_eye.x += 2;
      left_eye.height -= 5;
      right_eye.height -= 5;
      left_eye.height -= 1;
      left_eye.width -= 1;
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_LOOK_LEFT_RETURN_2;
        animProgress = 0;
      }
      break;

    case ANIM_LOOK_LEFT_RETURN_2:
      left_eye.x += 2;
      right_eye.x += 2;
      left_eye.height += 5;
      right_eye.height += 5;
      left_eye.height -= 1;
      left_eye.width -= 1;
      animProgress++;
      if (animProgress >= 3) {
        snapToCenter();
        animState = ANIM_IDLE;
        animProgress = 0;
      }
      break;

    // LOOK RIGHT TRANSITIONS:
    case ANIM_LOOK_RIGHT_TRANSITION_1:
      left_eye.x += 2;
      right_eye.x += 2;
      left_eye.height -= 5;
      right_eye.height -= 5;
      right_eye.height += 1;
      right_eye.width += 1;
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_LOOK_RIGHT_TRANSITION_2;
        animProgress = 0;
      }
      break;

    case ANIM_LOOK_RIGHT_TRANSITION_2:
      left_eye.x += 2;
      right_eye.x += 2;
      left_eye.height += 5;
      right_eye.height += 5;
      right_eye.height += 1;
      right_eye.width += 1;
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_LOOK_RIGHT_HOLD;
        animProgress = 0;
      }
      break;

    case ANIM_LOOK_RIGHT_HOLD:
      break;

    case ANIM_LOOK_RIGHT_RETURN_1:
      left_eye.x -= 2;
      right_eye.x -= 2;
      left_eye.height -= 5;
      right_eye.height -= 5;
      right_eye.height -= 1;
      right_eye.width -= 1;
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_LOOK_RIGHT_RETURN_2;
        animProgress = 0;
      }
      break;

    case ANIM_LOOK_RIGHT_RETURN_2:
      left_eye.x -= 2;
      right_eye.x -= 2;
      left_eye.height += 5;
      right_eye.height += 5;
      right_eye.height -= 1;
      right_eye.width -= 1;
      animProgress++;
      if (animProgress >= 3) {
        snapToCenter();
        animState = ANIM_IDLE;
        animProgress = 0;
      }
      break;

    // HAPPY TRANSITIONS:
    case ANIM_HAPPY_TRANSITION:
      animProgress++;
      if (animProgress >= 10) {
        animState = ANIM_HAPPY_HOLD;
        animProgress = 0;
      }
      break;

    case ANIM_HAPPY_HOLD:
      break;

    case ANIM_HAPPY_RETURN:
      animProgress++;
      if (animProgress >= 10) {
        snapToCenter();
        animState = ANIM_IDLE;
        animProgress = 0;
      }
      break;

    // SLEEPY TRANSITIONS:
    case ANIM_SLEEPING_TRANSITION:
      left_eye.height -= 2;
      right_eye.height -= 2;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 19) {
        animState = ANIM_SLEEPING_HOLD;
        animProgress = 0;
      }
      break;

    case ANIM_SLEEPING_HOLD:
      break;

    case ANIM_SLEEPING_RETURN:
      left_eye.height += 2;
      right_eye.height += 2;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 19) {
        snapToCenter();
        animState = ANIM_IDLE;
        animProgress = 0;
      }
      break;

    // SURPRISE TRANSITIONS:
    case ANIM_SURPRISE_TRANSITION:
      left_eye.height += 4;
      right_eye.height += 4;
      left_eye.width += 4;
      right_eye.width += 4;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 3) {
        animState = ANIM_SURPRISE_HOLD;
        animProgress = 0;
      }
      break;

    case ANIM_SURPRISE_HOLD:
      break;

    case ANIM_SURPRISE_RETURN:
      left_eye.height -= 4;
      right_eye.height -= 4;
      left_eye.width -= 4;
      right_eye.width -= 4;
      current_corner_radius = calculate_safe_radius(REF_CORNER_RADIUS, left_eye.width, left_eye.height);
      animProgress++;
      if (animProgress >= 3) {
        snapToCenter();
        animState = ANIM_IDLE;
        animProgress = 0;
      }
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

  // Draw happy cheeks overlay if in transition, hold or return
  int offset = -1; // Default: no cheeks
  if (animState == ANIM_HAPPY_TRANSITION) {
    offset = 20 - (animProgress * 2);
  } else if (animState == ANIM_HAPPY_HOLD) {
    offset = 0;
  } else if (animState == ANIM_HAPPY_RETURN) {
    offset = animProgress * 2;
  }

  if (offset >= 0) {
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
