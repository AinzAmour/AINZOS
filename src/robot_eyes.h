#ifndef ROBOT_EYES_H
#define ROBOT_EYES_H

#include "display.h"
#include "settings.h"
#include "clock_saver.h" // For RobotMood

// Reference constraints from original project
#define REF_EYE_HEIGHT 40
#define REF_EYE_WIDTH 40
#define REF_SPACE_BETWEEN_EYE 10
#define REF_CORNER_RADIUS 10

struct EyeState {
  int height;
  int width;
  int x;
  int y;
};

// Internal states for eye animation
enum EyeAnimState {
  ANIM_IDLE,
  
  ANIM_BLINK_CLOSING,
  ANIM_BLINK_OPENING,
  ANIM_BLINK_HOLD,
  
  ANIM_LOOK_LEFT_TRANSITION_1,
  ANIM_LOOK_LEFT_TRANSITION_2,
  ANIM_LOOK_LEFT_HOLD,
  ANIM_LOOK_LEFT_RETURN_1,
  ANIM_LOOK_LEFT_RETURN_2,
  
  ANIM_LOOK_RIGHT_TRANSITION_1,
  ANIM_LOOK_RIGHT_TRANSITION_2,
  ANIM_LOOK_RIGHT_HOLD,
  ANIM_LOOK_RIGHT_RETURN_1,
  ANIM_LOOK_RIGHT_RETURN_2,
  
  ANIM_HAPPY_TRANSITION,
  ANIM_HAPPY_HOLD,
  ANIM_HAPPY_RETURN,
  
  ANIM_SLEEPING_TRANSITION,
  ANIM_SLEEPING_HOLD,
  ANIM_SLEEPING_RETURN,
  
  ANIM_SURPRISE_TRANSITION,
  ANIM_SURPRISE_HOLD,
  ANIM_SURPRISE_RETURN
};

void initRobotEyes();
void updateRobotEyes(DisplayWrapper* display, SettingsManager* settings, RobotMood targetMood, uint32_t now);

#endif // ROBOT_EYES_H
