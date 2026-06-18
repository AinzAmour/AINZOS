#ifndef IMAGE_GALLERY_UI_H
#define IMAGE_GALLERY_UI_H

#include "display.h"
#include "buttons.h"

class ImageGalleryUI {
public:
  ImageGalleryUI(DisplayWrapper* disp);
  void enter();
  bool update(ButtonEvent btn);

private:
  DisplayWrapper* display;
  
  int currentImageIdx;
  bool fullscreen;
  bool slideshowActive;
  unsigned long lastSlideChangeMs;
  unsigned long slideDurationMs;

  void draw();
};

#endif // IMAGE_GALLERY_UI_H
