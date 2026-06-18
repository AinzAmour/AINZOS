#include "image_gallery_ui.h"
#include <Arduino.h>

const uint8_t image_ainzz[] PROGMEM = {
#include "../Images/ainzz.h"
};

const uint8_t image_retro1[] PROGMEM = {
#include "../Images/retro1.h"
};

static_assert(sizeof(image_ainzz) == 1024, "ainzz must be 128x64");
static_assert(sizeof(image_retro1) == 1024, "retro1 must be 128x64");

ImageGalleryUI::ImageGalleryUI(DisplayWrapper* disp)
  : display(disp), currentImageIdx(0), fullscreen(false),
    slideshowActive(false), lastSlideChangeMs(0), slideDurationMs(3000) {
}

void ImageGalleryUI::enter() {
  currentImageIdx = 0;
  fullscreen = false;
  slideshowActive = false;
  lastSlideChangeMs = millis();
  draw();
}

bool ImageGalleryUI::update(ButtonEvent btn) {
  if (btn == BTN_BACK || btn == BTN_BACK_LONG) {
    return false; // Exit back to Apps menu
  }

  bool changed = false;

  if (btn == BTN_UP) {
    currentImageIdx = (currentImageIdx == 0) ? 1 : 0;
    lastSlideChangeMs = millis();
    changed = true;
  } else if (btn == BTN_DOWN) {
    currentImageIdx = (currentImageIdx == 1) ? 0 : 1;
    lastSlideChangeMs = millis();
    changed = true;
  } else if (btn == BTN_SELECT) {
    fullscreen = !fullscreen;
    changed = true;
  } else if (btn == BTN_SELECT_LONG) {
    slideshowActive = !slideshowActive;
    lastSlideChangeMs = millis();
    changed = true;
  }

  if (slideshowActive && !changed) {
    if (millis() - lastSlideChangeMs >= slideDurationMs) {
      currentImageIdx = (currentImageIdx == 1) ? 0 : 1;
      lastSlideChangeMs = millis();
      changed = true;
    }
  }

  if (changed) {
    draw();
  }

  return true;
}

void ImageGalleryUI::draw() {
  display->clear();
  auto d = display->getDriver();

  const uint8_t* currentBitmap = (currentImageIdx == 0) ? image_ainzz : image_retro1;
  d->drawBitmap(0, 0, currentBitmap, 128, 64, SSD1306_WHITE);

  if (!fullscreen) {
    // Draw solid black bar overlay at the bottom 16 pixels
    d->fillRect(0, 48, 128, 16, SSD1306_BLACK);
    d->drawFastHLine(0, 48, 128, SSD1306_WHITE);

    d->setTextSize(1);
    d->setTextColor(SSD1306_WHITE);
    
    // Title
    d->setCursor(4, 52);
    d->print((currentImageIdx == 0) ? F("ainzz") : F("retro1"));

    // Slideshow AUTO label
    if (slideshowActive) {
      d->setCursor(52, 52);
      d->print(F("AUTO"));
    }

    // Counter
    d->setCursor(94, 52);
    d->printf("%02d/02", currentImageIdx + 1);
  }

  d->display();
}
