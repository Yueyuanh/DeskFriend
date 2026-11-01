#pragma once

#include <Arduino.h>

#include "face_state.h"
#include "mini_canvas.h"

// Board-owned 240x240 renderer. It draws the same expressions the browser page
// shows: the page sends the expression name, caption and clocks over HTTP, and
// this class replays DeskFriend_index.html drawScreen() on the LCD.
class FaceAnimation {
 public:
  bool begin();
  bool ready() const { return frame_ != nullptr; }
  void update(uint32_t nowMs, const FaceUpdate& request);

  uint32_t blinks() const { return blinks_; }
  uint32_t lastFlushUs() const { return lastFlushUs_; }

 private:
  void render(const FaceUpdate& request, float time, float age, bool blink);
  void pushFull();

  uint8_t* frame_ = nullptr;
  MiniCanvas canvas_{nullptr};
  uint32_t renderedSeq_ = 0;
  bool blinkRendered_ = false;
  uint32_t lastRenderMs_ = 0;
  uint32_t blinks_ = 0;
  uint32_t lastFlushUs_ = 0;
};
