#include "text_input_ui.h"
#include <string.h>
#include <stdio.h>

TextInputUI::TextInputUI(DisplayWrapper* disp, char* buffer, size_t bufLen, const char* title_, bool hide_)
  : display(disp), buf(buffer), bufLen(0), len(0), pos(0), hide(hide_), title(title_), finished(false), cancelled(false) {
  charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_@.";
  if (buf && bufLen > 0) {
    // Clamp external buffer length to our safe maximum (include space for NUL)
    this->bufLen = (bufLen > (MAX_INPUT_LEN + 1)) ? (MAX_INPUT_LEN + 1) : bufLen;
    buf[0] = '\0';
  }
}

TextInputUI::~TextInputUI() {
}

void TextInputUI::enter() {
  len = (int)strnlen(buf, bufLen);
  if (len >= (int)bufLen - 1) len = (int)bufLen - 1;
  pos = len;
  finished = false;
  cancelled = false;
  redraw();
}

int TextInputUI::charsetIndexOf(char c) {
  const char* p = strchr(charset, c);
  if (!p) return 0;
  return (int)(p - charset);
}

void TextInputUI::redraw() {
  display->clear();
  display->drawHeader(title ? title : "Input");
  Adafruit_SSD1306* d = display->getDriver();
  d->setTextSize(1);
  d->setTextColor(SSD1306_WHITE);
  d->setCursor(2, 24);
  // Draw buffer (masked if hide)
  char line[40];
  if (len < 0) len = 0;
  if (pos < 0) pos = 0;
  if (pos > len) pos = len;
  if (hide) {
    int m = (len < (int)sizeof(line)-1) ? len : (int)sizeof(line)-1;
    for (int i = 0; i < m; i++) line[i] = '*';
    line[m] = '\0';
  } else {
    strncpy(line, buf, sizeof(line)-1);
    line[sizeof(line)-1] = '\0';
  }
  d->print(line);

  // Draw cursor as an inverted block under the current char
  int cursorX = 2 + (pos * 6);
  int cursorY = 34;
  if (cursorX < 126) {
    d->fillRect(cursorX, cursorY, 6, 8, SSD1306_WHITE);
    d->setTextColor(SSD1306_BLACK);
    if (pos < len) {
      char ch[2] = { buf[pos], '\0' };
      if (hide) ch[0] = '*';
      d->setCursor(cursorX, cursorY);
      d->print(ch);
    }
    d->setTextColor(SSD1306_WHITE);
  }

  // Footer hints
  d->setTextSize(1);
  d->setCursor(2, 52);
  d->print(F("UP/DN: +/- char  SEL: next"));
  d->setCursor(2, 58);
  d->print(F("Hold UP/DN: pos  LP SEL: ok"));

  d->display();
}

bool TextInputUI::update(ButtonEvent btn) {
  if (finished || cancelled) return false;

  bool keepActive = true;
  if (btn == BTN_UP) {
    if (pos == len) {
      if (len < (int)bufLen - 1) {
        buf[len++] = charset[0];
        buf[len] = '\0';
      } else {
        return keepActive;
      }
    }
    int idx = charsetIndexOf(buf[pos]);
    idx = (idx + 1) % (int)strlen(charset);
    buf[pos] = charset[idx];
    redraw();
  } else if (btn == BTN_DOWN) {
    if (pos == len) {
      if (len < (int)bufLen - 1) {
        buf[len++] = charset[0];
        buf[len] = '\0';
      } else {
        return keepActive;
      }
    }
    int idx = charsetIndexOf(buf[pos]);
    idx = (idx - 1 + (int)strlen(charset)) % (int)strlen(charset);
    buf[pos] = charset[idx];
    redraw();
  } else if (btn == BTN_UP_LONG) {
    if (pos > 0) pos--;
    redraw();
  } else if (btn == BTN_DOWN_LONG) {
    if (pos < len) pos++;
    redraw();
  } else if (btn == BTN_SELECT) {
    if (pos < len) {
      pos++;
    } else if (pos == len && len < (int)bufLen - 1) {
      buf[len++] = charset[0];
      buf[len] = '\0';
      pos = len;
    }
    redraw();
  } else if (btn == BTN_SELECT_LONG) {
    finished = true;
    keepActive = false;
  } else if (btn == BTN_BACK) {
    if (pos > 0) {
      for (int i = pos - 1; i < len - 1; i++) {
        buf[i] = buf[i + 1];
      }
      buf[--len] = '\0';
      pos--;
      redraw();
    } else if (len == 0) {
      cancelled = true;
      keepActive = false;
    }
  } else if (btn == BTN_BACK_LONG) {
    cancelled = true;
    keepActive = false;
  }

  return keepActive;
}
