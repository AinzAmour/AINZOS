#ifndef TEXT_INPUT_UI_H
#define TEXT_INPUT_UI_H

#include "display.h"
#include "buttons.h"
#include <stddef.h>

class TextInputUI {
public:
  static const size_t MAX_INPUT_LEN = 32;
  TextInputUI(DisplayWrapper* disp, char* buffer, size_t bufLen, const char* title = "Input", bool hide = false);
  ~TextInputUI();
  void enter();
  // Returns true while still active, false when finished (confirmed or cancelled)
  bool update(ButtonEvent btn);
  bool isFinished() const { return finished; }
  bool wasCancelled() const { return cancelled; }
  const char* getText() const { return buf; }

private:
  DisplayWrapper* display;
  char* buf;
  size_t bufLen;
  int len;
  int pos;
  bool hide;
  const char* title;
  bool finished;
  bool cancelled;
  const char* charset;
  int charsetIndexOf(char c);
  void redraw();
};

#endif
