#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// The web page owns the expression timeline; the board mirrors it.
// Only semantic expression, caption, pose and clock data are transferred (<4 KB),
// never pixels: the LCD is still rendered on the ESP32.
struct FaceUpdate {
  char face[16];   // "idle", "happy", "clock", ...
  char text[256];  // UTF-8; room for dynamic nicknames and captions
  float time;      // page global clock, seconds (drives sine animations)
  float age;       // seconds since this expression started
  bool blink;      // the page is blinking right now
  float blinkRemaining;
  bool hold;       // long-press progress ring is active
  float holdProgress;
  float holdRate;
  // Browser wall clock, so the board's clock face shows exactly the same time.
  uint8_t clockHour;
  uint8_t clockMinute;
  uint8_t clockSecond;
  uint8_t clockWeekday;
  uint8_t clockMonth;
  uint8_t clockDay;
  float clockSeconds;  // seconds within the minute, with milliseconds
  double wallClockMs;
  int16_t utcOffsetMinutes;
  // Full semantic state for the two formerly approximate views. No pixels.
  char chatName[64];
  char chatLines[12][192];
  uint8_t chatLineCount;
  uint8_t chatPageLines[4];
  float chatStarts[4];
  uint8_t chatPageCount;
  char stick[16];
  bool cameraPose;
  float joints[15][2]; // head,neck,sL,sR,hip,eL,eR,hL,hR,kL,kR,fL,fR + reserved
  char stickEyes[12];
  char stickMouth[12];
  bool paused;          // deterministic preview / animation pause
  bool networkConnected; // device-owned, never accepted from browser JSON
  uint32_t clientSeq;
  uint32_t seq;        // increments on every accepted update
  uint32_t updatedMs;  // board millis() when the update landed
};

class FaceRequestState {
 public:
  void update(const FaceUpdate& update);
  FaceUpdate snapshot();
  void displayed(uint32_t seq, uint32_t renderUs, uint32_t flushUs);
  void metrics(uint32_t& seq, uint32_t& renderUs, uint32_t& flushUs);

 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  FaceUpdate value_{};
  uint32_t seq_ = 0;
  uint32_t displayedSeq_ = 0, renderUs_ = 0, flushUs_ = 0;
};

extern FaceRequestState faceRequest;
