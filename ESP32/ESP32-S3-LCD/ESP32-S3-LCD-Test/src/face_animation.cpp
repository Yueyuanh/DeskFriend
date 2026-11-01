#include "face_animation.h"

#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include <time.h>

#include "DEV_Config.h"
#include "LCD_1in28.h"
#include "face_frame.h"
#include "device_state.h"

namespace {
constexpr int kBytes = MiniCanvas::kBytes;
constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 6.2831853f;

// Same palette as DeskFriend_index.html.
constexpr uint32_t kBg = 0x0c171b;       // #0c171b
constexpr uint32_t kCream = 0xf3efe0;    // #f3efe0
constexpr uint32_t kCoral = 0xff7a6b;    // #ff7a6b
constexpr uint32_t kWhite = 0xffffff;
constexpr uint32_t kBlue = 0x6fd0ff;     // #6fd0ff
constexpr uint32_t kCyan = 0x8fe3d6;     // #8fe3d6
constexpr uint32_t kLime = 0xe6ff4d;     // #e6ff4d
constexpr uint32_t kYellow = 0xffe14d;   // #ffe14d
constexpr uint32_t kOrange = 0xff9a3d;   // #ff9a3d
constexpr uint32_t kSun = 0xffd35e;      // #ffd35e
constexpr uint32_t kGreen = 0x8fe38a;    // #8fe38a
constexpr uint32_t kRed = 0xff5a4d;      // #ff5a4d
constexpr uint32_t kNight = 0x7a8cff;    // #7a8cff
constexpr uint32_t kPink = 0xff6f9a;     // #ff6f9a
constexpr uint32_t kRose = 0xff7aa8;     // #ff7aa8
constexpr uint32_t kPeach = 0xff9a8f;    // #ff9a8f
constexpr uint32_t kMoon = 0xffe9a8;     // #ffe9a8
constexpr uint32_t kClockInk = 0xfff6e8; // #fff6e8
constexpr uint32_t kPaper = 0xffffff;    // #fff
constexpr uint32_t kEyeBlue = 0x6fa8ff;  // #6fa8ff
constexpr uint32_t kLoveRose = 0xff9ab8; // #ff9ab8
constexpr uint32_t kGreetInk = 0xfff6e8; // #fff6e8
constexpr uint32_t kChatAvatar = 0x2f5d57;  // #2f5d57
constexpr uint32_t kChatBubble = 0xe9eff0;  // #e9eff0
constexpr uint32_t kChatInk = 0x10201f;     // #10201f
constexpr uint32_t kChatDim = 0x7d8f93;     // #7d8f93

inline float clamp01(float value) { return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }

uint32_t glowColor(const char* face) {
  if (!strcmp(face, "happy") || !strcmp(face, "off") || !strcmp(face, "energy") ||
      !strcmp(face, "dance") || !strcmp(face, "charging") || !strcmp(face, "greeting") ||
      !strcmp(face, "gsend") || !strcmp(face, "replyDone"))
    return kLime;
  if (!strcmp(face, "shy") || !strcmp(face, "thanks")) return kPeach;
  if (!strcmp(face, "pressed") || !strcmp(face, "no")) return kCoral;
  if (!strcmp(face, "eat") || !strcmp(face, "wink")) return 0xffc15e;
  if (!strcmp(face, "water") || !strcmp(face, "msg") || !strcmp(face, "scared"))
    return kBlue;
  if (!strcmp(face, "night")) return kNight;
  if (!strcmp(face, "think") || !strcmp(face, "sleepy")) return 0x9aa8ff;
  if (!strcmp(face, "miss")) return kRose;
  if (!strcmp(face, "cheer") || !strcmp(face, "dizzy")) return kOrange;
  if (!strcmp(face, "wow")) return kYellow;
  if (!strcmp(face, "sad")) return 0x6f8cb8;
  if (!strcmp(face, "love")) return kPink;
  if (!strcmp(face, "lowbat")) return kRed;
  if (!strcmp(face, "full")) return kGreen;
  if (!strcmp(face, "offline")) return 0x8896a0;
  if (!strcmp(face, "grecv")) return kSun;
  if (!strcmp(face, "chat")) return kBlue;
  if (!strcmp(face, "stick")) return kWhite;
  // idle, curious, clock, boot, greeting, gsend, replyWait, replyDone
  return kCyan;
}

float blushFactor(const char* face) {
  if (!strcmp(face, "shy")) return 1.3f;
  if (!strcmp(face, "thanks")) return 1.2f;
  if (!strcmp(face, "love")) return 1.15f;
  if (!strcmp(face, "sad")) return 0.35f;
  if (!strcmp(face, "wow")) return 0.6f;
  if (!strcmp(face, "night") || !strcmp(face, "sleepy")) return 0.7f;
  return 1.0f;
}

void eye(MiniCanvas& c, float x, float y, float w, float h) {
  c.fillEllipse(x, y, w, fmaxf(h, 2.0f), kCream);
}

void arc9(MiniCanvas& c, float x, float y, float r, float start, float end) {
  c.strokeArc(x, y, r, 4.5f, start, end, kCream);
}

void happyEyes(MiniCanvas& c, float y, float r) {
  arc9(c, 82, y, r, kPi * 1.1f, kPi * 1.9f);
  arc9(c, 158, y, r, kPi * 1.1f, kPi * 1.9f);
}

void star(MiniCanvas& c, float x, float y, float s, uint32_t color) {
  float xs[8];
  float ys[8];
  for (int i = 0; i < 8; ++i) {
    const float r = (i % 2) ? s * 0.35f : s;
    const float a = i * kPi / 4.0f - kPi / 2.0f;
    xs[i] = x + r * cosf(a);
    ys[i] = y + r * sinf(a);
  }
  c.fillPolygon(xs, ys, 8, color);
}

void heart(MiniCanvas& c, float x, float y, float s, uint32_t color) { c.fillHeart(x,y,s,color); }

void drop(MiniCanvas& c, float x, float y, float s, uint32_t color) { c.fillDrop(x,y,s,color); }

void fillArc(MiniCanvas& c, float x,float y,float r,float start,float end,uint32_t color) { c.fillArc(x,y,r,start,end,color); }

void zz(MiniCanvas& c, float time) {
  for (int i = 0; i < 2; ++i) {
    const float p = fmodf(time * 0.5f + i * 0.5f, 1.0f);
    c.setAlpha(sinf(kPi * p));
    c.fillText(150 + p * 24 + i * 4, 100 - p * 40, "Z", 12 + 8*p, 0x9aa8ff, MiniCanvas::kCenter,
               MiniCanvas::kAlphabetic);
  }
  c.setAlpha(1.0f);
}

void battery(MiniCanvas& c, float cx, float cy, float level, uint32_t color) {
  c.strokeRect(cx - 38, cy - 16, 72, 32, 3, kCream);
  c.fillRect(cx + 36, cy - 6, 6, 12, kCream);
  c.fillRect(cx - 32, cy - 10, 60 * fmaxf(0.0f, fminf(1.0f, level)), 20, color);
}

// A wavy stroke made of two flattened quadratic curves, used by several faces.
void wavyMouth(MiniCanvas& c,float x0,float y0,float cx,float cy,float x1,float y1,float cx2,float cy2,float x2,float y2) { c.strokeQuadratics(x0,y0,cx,cy,x1,y1,cx2,cy2,x2,y2,4.5f,kCream); }

void drawFace(MiniCanvas& c, const char* face, const char* text, float T, float age,
              bool blink) {
  const float b = blink ? 0.12f : 1.0f;
  if (!strcmp(face, "happy")) {
    happyEyes(c, 112, 22);
    arc9(c, 120, 128, 36, kPi * 0.12f, kPi * 0.88f);
  } else if (!strcmp(face, "curious")) {
    eye(c, 78, 104, 17, 22 * b);
    eye(c, 162, 98, 25, 31 * b);
    c.strokeEllipse(120, 168, 9, 12, 4.5f, kCream);
  } else if (!strcmp(face, "shy")) {
    arc9(c, 82, 96, 20, kPi * 0.1f, kPi * 0.9f);
    arc9(c, 158, 96, 20, kPi * 0.1f, kPi * 0.9f);
    wavyMouth(c, 98, 166, 110, 156, 120, 166, 130, 176, 142, 166);
  } else if (!strcmp(face, "pressed")) {
    happyEyes(c, 104, 22);
    fillArc(c, 120, 124, 30, 0, kPi, kCream);
    c.fillHalfEllipse(120, 148, 14, 8, kCoral);
    star(c, 48, 64, 8 + 3 * sinf(T * 10), kYellow);
    star(c, 192, 64, 8 + 3 * sinf(T * 10 + 2), kYellow);
  } else if (!strcmp(face, "greeting")) {
    eye(c, 82, 100, 20, 26 * b);
    eye(c, 158, 100, 20, 26 * b);
    fillArc(c, 120, 142, 34, 0, kPi, kCream);
    fillArc(c,120,168,16,0,kPi,kCoral);
    if (text[0] == '\0') {
      c.fillText(120, 212, "Hi!", 26, kCream, MiniCanvas::kCenter, MiniCanvas::kAlphabetic);
    }
  } else if (!strcmp(face, "eat")) {
    happyEyes(c, 100, 18);
    c.fillEllipse(120, 138, 16, 5 + 9 * fabsf(sinf(T * 11)), kCoral);
    fillArc(c, 120, 160, 42, 0, kPi, 0xf6eedd);  // #f6eedd
    fillArc(c, 120, 160, 32, kPi, kTwoPi, kPaper);
    c.strokeLine(138, 150, 172, 112 + 2 * sinf(T * 11), 2.5f, 0xffc15e);
    c.strokeLine(148, 156, 182, 120 + 2 * sinf(T * 11), 2.5f, 0xffc15e);
  } else if (!strcmp(face, "water")) {
    eye(c, 82, 104, 14, 20 * b);
    eye(c, 158, 104, 14, 20 * b);
    arc9(c, 120, 136, 20, kPi * 0.15f, kPi * 0.85f);
    drop(c, 186, 62 + 4 * sinf(T * 4), 16, kBlue);
    c.setAlpha(0.6f);
    drop(c, 52, 40 + fmodf(T * 50, 70), 9, kBlue);
    c.setAlpha(1.0f);
  } else if (!strcmp(face, "off")) {
    happyEyes(c, 102, 19);
    fillArc(c, 120, 126, 30, 0, kPi, kCream);
    c.fillEllipse(120, 146, 14, 8, kCoral);
    for (int i = 0; i < 7; ++i) {
      const float x = 30 + i * 30;
      const float y = fmodf(T * 70 + i * 43, 150) + 8;
      const uint32_t color = (i % 3 == 0) ? kCoral : (i % 3 == 1 ? kLime : kCyan);
      c.save(); c.translate(x,y); c.rotate(T*4+i); c.fillRect(-4,-2,8,4,color); c.restore();
    }
  } else if (!strcmp(face, "energy")) {
    const float s = 20 + 3 * sinf(T * 10);
    star(c, 82, 100, s, kCream);
    star(c, 158, 100, s, kCream);
    arc9(c, 120, 128, 28, kPi * 0.12f, kPi * 0.88f);
    const float boltX[6] = {182, 162, 176, 166, 196, 180};
    const float boltY[6] = {34, 70, 70, 102, 60, 60};
    float xs[6];
    float ys[6];
    for (int i = 0; i < 6; ++i) {
      xs[i] = boltX[i];
      ys[i] = boltY[i] + 2 * sinf(T * 14);
    }
    c.fillPolygon(xs, ys, 6, kLime);
  } else if (!strcmp(face, "night")) {
    arc9(c, 82, 104, 18, kPi * 0.12f, kPi * 0.88f);
    arc9(c, 158, 104, 18, kPi * 0.12f, kPi * 0.88f);
    arc9(c, 120, 138, 10, kPi * 0.15f, kPi * 0.85f);
    c.fillEllipse(184, 58, 16, 16, kMoon);
    c.fillEllipse(191, 53, 14, 14, kBg);
    zz(c, T);
  } else if (!strcmp(face, "thanks")) {
    happyEyes(c, 102, 18);
    arc9(c, 120, 128, 24, kPi * 0.15f, kPi * 0.85f);
    star(c, 46, 60, 10 + 3 * sinf(T * 6), kYellow);
    star(c, 198, 64, 8 + 3 * sinf(T * 6 + 2), kYellow);
    star(c, 120, 48, 7 + 2 * sinf(T * 6 + 4), kPeach);
  } else if (!strcmp(face, "miss")) {
    for (int i = 0; i < 2; ++i) {
      const float x = i == 0 ? 82 : 158;
      eye(c, x, 104, 17, 22 * b);
      c.fillEllipse(x, 118, 13, 5, kEyeBlue);
    }
    wavyMouth(c, 104, 146, 112, 138 + 2 * sinf(T * 8), 120, 146, 128, 154, 136, 146);
    for (int i = 0; i < 2; ++i) {
      const float p = fmodf(T * 0.45f + i * 0.5f, 1.0f);
      c.setAlpha(sinf(kPi * p));
      heart(c, 100 + i * 40 + 8 * sinf(T * 3 + i), 70 - p * 34, 10 + 4 * p, kRose);
    }
    c.setAlpha(1.0f);
  } else if (!strcmp(face, "msg")) {
    eye(c, 82, 104, 15, 22 * b);
    eye(c, 158, 104, 15, 22 * b);
    c.strokeEllipse(120, 146, 8, 10, 4.5f, kCream);
    c.save(); c.translate(168,56); c.rotate(.18f*sinf(T*18)*(age<1.2f?1:.2f));
    c.fillRect(-24,-16,48,32,kCream);
    const float ex[3]={-24,0,24}, ey[3]={-15,3,-15};
    c.strokePolyline(ex,ey,3,1.5f,0x2a4a55); c.restore();
    c.fillEllipse(188, 42, 7 + 2 * sinf(T * 8), 7 + 2 * sinf(T * 8), kCoral);
  } else if (!strcmp(face, "cheer")) {
    c.strokeLine(62, 78, 100, 88, 3.5f, kCream);
    c.strokeLine(178, 78, 140, 88, 3.5f, kCream);
    eye(c, 82, 106, 15, 19 * b);
    eye(c, 158, 106, 15, 19 * b);
    fillArc(c, 120, 128, 26, 0, kPi, kCream);
    c.fillEllipse(120, 146, 12, 7, kCoral);
    star(c, 40, 66, 9 + 3 * sinf(T * 9), 0xffb35e);
    star(c, 200, 70, 8 + 3 * sinf(T * 9 + 2), 0xffb35e);
  } else if (!strcmp(face, "sleepy")) {
    c.strokeLine(64, 104 + 2 * sinf(T * 2), 100, 110, 4.0f, kCream);
    c.strokeLine(176, 104 + 2 * sinf(T * 2), 140, 110, 4.0f, kCream);
    const float y = 0.5f + 0.5f * sinf(T * 1.6f);
    c.strokeEllipse(120, 148, 8 + 8 * y, 6 + 12 * y, 4.0f, kCream);
    zz(c, T);
  } else if (!strcmp(face, "wow")) {
    for (int i = 0; i < 2; ++i) {
      const float x = i == 0 ? 82 : 158;
      c.fillEllipse(x, 100, 22, 28 * b, kCream);
      c.fillEllipse(x, 100, 8, 8, kBg);
    }
    c.strokeEllipse(120, 150, 12, 16, 4.5f, kCream);
    c.fillText(180, 72, "!", 38 + 4*sinf(T*12), kYellow, MiniCanvas::kCenter, MiniCanvas::kAlphabetic);
  } else if (!strcmp(face, "sad")) {
    c.strokeLine(62, 82, 98, 70, 3.5f, kCream);
    c.strokeLine(178, 82, 142, 70, 3.5f, kCream);
    eye(c, 82, 108, 14, 17 * b);
    eye(c, 158, 108, 14, 17 * b);
    c.fillEllipse(66, 128 + fmodf(T * 0.6f, 1.0f) * 20, 5, 7, kEyeBlue);
    arc9(c, 120, 176, 24, kPi * 1.2f, kPi * 1.8f);
  } else if (!strcmp(face, "love")) {
    const float s = 22 + 2.5f * sinf(T * 8);
    heart(c, 82, 100, s, kPink);
    heart(c, 158, 100, s, kPink);
    arc9(c, 120, 134, 24, kPi * 0.15f, kPi * 0.85f);
    for (int i = 0; i < 3; ++i) {
      const float p = fmodf(T * 0.5f + i / 3.0f, 1.0f);
      c.setAlpha(sinf(kPi * p));
      heart(c, 60 + i * 60 + 6 * sinf(T * 3 + i), 58 - p * 20, 8 + 5 * p, kLoveRose);
    }
    c.setAlpha(1.0f);
  } else if (!strcmp(face, "wink")) {
    arc9(c, 82, 106, 17, kPi * 1.1f, kPi * 1.9f);
    eye(c, 158, 104, 16, 23 * b);
    arc9(c, 120, 134, 26, kPi * 0.12f, kPi * 0.88f);
    c.fillHalfEllipse(132, 154, 8, 6, kCoral);
    star(c, 194, 76, 9 + 3 * sinf(T * 9), kYellow);
  } else if (!strcmp(face, "think")) {
    for (int i = 0; i < 2; ++i) {
      const float x = i == 0 ? 82 : 158;
      c.fillEllipse(x, 102, 16, 22 * b, kCream);
      c.fillEllipse(x - 6, 94, 6, 6, kBg);
    }
    c.strokeLine(104, 148, 128, 144, 4.5f, kCream);
    for (int i = 0; i < 3; ++i) {
      const float r = 3 + i * 1.5f + (sinf(T * 4 - i) > 0.6f ? 1.5f : 0.0f);
      c.fillEllipse(150 + i * 14, 58 - i * 5, r, r, kCream);
    }
  } else if (!strcmp(face, "scared")) {
    for (int i = 0; i < 2; ++i) {
      const float x = i == 0 ? 82 : 158;
      c.fillEllipse(x, 100, 20, 26 * b, kCream);
      c.fillEllipse(x + sinf(T * 30) * 2, 102, 5, 5, kBg);
    }
    float xs[6] = {98, 106, 114, 122, 130, 138};
    float ys[6] = {150, 144, 156, 144, 156, 144};
    c.strokePolyline(xs, ys, 6, 4.5f, kCream);
    drop(c, 190, 70 + 3 * sinf(T * 5), 9, kBlue);
  } else if (!strcmp(face, "dance")) {
    happyEyes(c, 100, 18);
    fillArc(c, 120, 124, 28, 0, kPi, kCream);
    c.fillEllipse(120, 144, 12, 7, kCoral);
    for (int i = 0; i < 2; ++i) {
      const float p = fmodf(T * 0.8f + i * 0.5f, 1.0f);
      c.setAlpha(sinf(kPi * p));
      c.fillText(50 + i * 140 + 6 * sinf(T * 5 + i), 92 - p * 36, i ? "♫" : "♪",
                 30, kLime, MiniCanvas::kCenter, MiniCanvas::kAlphabetic);
    }
    c.setAlpha(1.0f);
  } else if (!strcmp(face, "dizzy")) {
    for (int i = 0; i < 2; ++i) {
      const float x = i == 0 ? 82 : 158;
      float xs[35];
      float ys[35];
      int count = 0;
      for (float a = 0; a < 12 && count < 35; a += 0.35f) {
        const float r = 2 + a * 1.7f;
        xs[count] = x + r * cosf(a + T * 6);
        ys[count] = 102 + r * sinf(a + T * 6);
        ++count;
      }
      c.strokePolyline(xs, ys, count, 2.0f, kCream);
    }
    c.strokeCubic(98,150,108,138+6*sinf(T*5),132,162-6*sinf(T*5),142,150,4.5f,kCream);
  } else if (!strcmp(face, "no")) {
    c.strokeLine(60, 74, 100, 88, 3.5f, kCream);
    c.strokeLine(180, 74, 140, 88, 3.5f, kCream);
    eye(c, 82, 106, 12, 13 * b);
    eye(c, 158, 106, 12, 13 * b);
    wavyMouth(c, 100, 152, 110, 144, 120, 152, 130, 160, 140, 152);
  } else if (!strcmp(face, "lowbat")) {
    c.strokeLine(64, 100, 100, 108, 4.5f, kCream);
    c.strokeLine(176, 100, 140, 108, 4.5f, kCream);
    battery(c, 120, 140, 0.12f + 0.02f * sinf(T * 5),
            sinf(T * 5) > -0.2f ? kRed : 0x7a2a2a);
  } else if (!strcmp(face, "charging")) {
    happyEyes(c, 98, 17);
    battery(c, 120, 140, fmodf(T * 0.35f, 1.0f), kLime);
  } else if (!strcmp(face, "full")) {
    happyEyes(c, 98, 17);
    battery(c, 120, 140, 1.0f, kGreen);
    star(c, 52, 70, 10 + 3 * sinf(T * 6), kYellow);
    star(c, 188, 70, 9 + 3 * sinf(T * 6 + 2), kYellow);
  } else if (!strcmp(face, "boot")) {
    const float o = fminf(1.0f, age / 0.9f);
    eye(c, 82, 104, 16, 23 * o * b);
    eye(c, 158, 104, 16, 23 * o * b);
    c.strokeArc(120, 150, 14, 3.5f, kPi * 1.5f + 0.7f, kPi * 3.5f - 0.7f, kCream);
    c.strokeLine(120, 134, 120, 150, 3.5f, kCream);
  } else if (!strcmp(face, "offline")) {
    c.strokeLine(64, 104, 100, 104, 4.5f, kCream);
    c.strokeLine(176, 104, 140, 104, 4.5f, kCream);
    for (float r = 10; r <= 30; r += 10) {
      c.strokeArc(120, 160, r, 3.0f, kPi * 1.25f, kPi * 1.75f, kCream);
    }
    c.fillEllipse(120, 160, 3, 3, kCream);
    c.strokeLine(96, 170, 144, 128, 3.0f, kCoral);
  } else {
    eye(c,82,104,16,23*b); eye(c,158,104,16,23*b);
    arc9(c,120,142,20,kPi*.15f,kPi*.85f);
  }
}

// Port of drawClock(): the browser's wall clock arrives in the FaceUpdate.
void drawClockFace(MiniCanvas& c, float T, bool blink, const FaceUpdate& request) {
  const int h = request.clockHour;
  const int m = request.clockMinute;
  const float s = request.clockSeconds;
  const bool night = h >= 22 || h < 6;
  const bool morning = h >= 6 && h < 9;
  const float hourFraction = h + m / 60.0f;
  const bool lunch = hourFraction >= 11.5f && hourFraction < 13.0f;
  const bool pop = s < 1.4f;
  const uint32_t col = night ? kNight : kCyan;
  const uint32_t ink = kCream;

  c.setAlpha(0.12f);
  c.strokeArc(120, 120, 114, 3, 0, kTwoPi, kWhite);
  c.setAlpha(1.0f);
  c.strokeArc(120, 120, 114, 3, -kPi / 2, -kPi / 2 + kTwoPi * s / 60.0f, col);

  const float gx = 5 * sinf(T * 0.6f) * fmaxf(0.0f, sinf(T * 0.17f + 1.0f));
  for (int i = 0; i < 2; ++i) {
    const float x = i == 0 ? 96 : 144;
    if (night) {
      c.strokeArc(x, 62, 8, 3, kPi * 0.12f, kPi * 0.88f, ink);
    } else if (pop) {
      c.strokeArc(x, 66, 8, 3, kPi * 1.1f, kPi * 1.9f, ink);
    } else {
      c.fillEllipse(x + gx, 62, 6, blink ? 1.5f : 9, ink);
    }
  }
  c.strokeArc(120, pop ? 74 : 78, pop ? 14 : 10, 2.5f, kPi * 0.2f, kPi * 0.8f, ink);
  c.setAlpha(0.45f);
  c.fillEllipse(74, 80, 9, 5, kCoral);
  c.fillEllipse(166, 80, 9, 5, kCoral);
  c.setAlpha(1.0f);

  if (night) {
    c.fillEllipse(176, 58, 10, 10, kMoon);
    c.fillEllipse(181, 54, 9, 9, kBg);
    c.setAlpha(0.4f + 0.6f * fabsf(sinf(T * 1.3f)));
    c.fillText(62, 54, "z", 14, 0x9aa8ff, MiniCanvas::kCenter, MiniCanvas::kAlphabetic);
    c.setAlpha(0.4f + 0.6f * fabsf(sinf(T * 1.3f + 1)));
    c.fillText(52, 42, "Z", 14, 0x9aa8ff, MiniCanvas::kCenter, MiniCanvas::kAlphabetic);
    c.setAlpha(1.0f);
  } else if (morning) {
    c.fillEllipse(176, 58, 5, 5, kSun);
    for (int i = 0; i < 8; ++i) {
      const float a = i * kPi / 4 + T * 0.5f;
      c.strokeLine(176 + 9 * cosf(a), 58 + 9 * sinf(a), 176 + 13 * cosf(a),
                   58 + 13 * sinf(a), 1.5f, kSun);
    }
  } else if (lunch) {
    fillArc(c, 176, 66, 11, 0, kPi, 0xf6eedd);
    c.setAlpha(0.7f);
    for (int k = -5; k <= 5; k += 5) {
      float xs[16];
      float ys[16];
      const int n = MiniCanvas::flattenQuadratic(176 + k, 52,
                                                 176 + k + 3 * sinf(T * 3 + k), 47,
                                                 176 + k, 42, xs, ys, 16);
      c.strokePolyline(xs, ys, n, 1.25f, kCream);
    }
    c.setAlpha(1.0f);
  }

  char str[9];
  snprintf(str, sizeof(str), "%02d:%02d", h, m);
  const int cw[5] = {32, 32, 16, 32, 32};
  float x = 48;
  for (int i = 0; i < 5; ++i) {
    c.setAlpha((i == 2 && fmodf(s, 1.0f) > 0.5f) ? 0.25f : 1.0f);
    char glyph[2] = {str[i], '\0'};
    c.fillText(x + cw[i] / 2.0f, 124, glyph, 52, kClockInk, MiniCanvas::kCenter,
               MiniCanvas::kMiddle);
    x += cw[i];
  }
  c.setAlpha(1.0f);

  static const char* kWeekdays[7] = {"日", "一", "二", "三",
                                     "四", "五", "六"};
  char date[32];
  snprintf(date, sizeof(date), "周%s  %d/%d",
           kWeekdays[request.clockWeekday % 7], request.clockMonth, request.clockDay);
  c.fillText(120, 168, date, 15, col, MiniCanvas::kCenter, MiniCanvas::kMiddle);
  const char* greeting = night ? "夜深了，早点休息"
                               : lunch ? "该吃饭啦"
                                       : hourFraction < 11.5f
                                             ? "早上好"
                                             : h < 18 ? "下午好"
                                                      : "晚上好";
  c.setAlpha(0.7f);
  c.fillText(120, 192, greeting, 13, kCream, MiniCanvas::kCenter, MiniCanvas::kMiddle, true);
  c.setAlpha(1.0f);
}

// ---------- 拍一拍：发送光波 / 接收轻拍 / 回传开心反馈 ----------
void drawGreet(MiniCanvas& c, const char* face, const char* cap, float T, float age,
               float progress) {
  const bool wait = !strcmp(face, "replyWait");
  const bool done = !strcmp(face, "replyDone");
  if (!strcmp(face, "gsend")) {
    for (int i = 0; i < 3; ++i) {
      const float p = fmodf(age * 0.8f + i / 3.0f, 1.0f);
      c.setAlpha((1.0f - p) * 0.85f);
      c.strokeArc(120, 100, 26 + p * 78, (5 * (1 - p) + 1) * 0.5f, 0, kTwoPi, kLime);
    }
    c.setAlpha(1.0f);
    const float s = 0.75f + 0.25f * fminf(1.0f, age / 0.3f) + 0.04f * sinf(T * 8);
    c.save();
    c.translate(120, 100);
    c.scale(s);
    c.strokeArc(-26, -4, 15, 4.0f, kPi * 1.1f, kPi * 1.9f, kCream);
    c.strokeArc(26, -4, 15, 4.0f, kPi * 1.1f, kPi * 1.9f, kCream);
    fillArc(c, 0, 8, 24, 0, kPi, kCream);
    c.fillHalfEllipse(0,28,11,6,kCoral);
    c.setAlpha(0.6f);
    c.fillEllipse(-48, 14, 12, 7, kCoral);
    c.fillEllipse(48, 14, 12, 7, kCoral);
    c.setAlpha(1.0f);
    c.restore();
    for (int i = 0; i < 3; ++i) {
      const float p = fmodf(age * 0.9f + i / 3.0f, 1.0f);
      c.setAlpha(sinf(kPi * p));
      heart(c, 70 + i * 50 + 6 * sinf(T * 3 + i), 70 - p * 22, 8 + 4 * p, kCoral);
    }
    c.setAlpha(1.0f);
    if (age < 1.6f) {
      char line[96];
      const int dots = 1 + (static_cast<int>(T * 3) % 3);
      snprintf(line, sizeof(line), "%s%.*s", cap, dots, "...");
      c.fillText(120, 190, line, 16, kGreetInk, MiniCanvas::kCenter,
                 MiniCanvas::kMiddle);
    } else {
      c.fillText(120, 190, "拍一拍已发出 ✓", 16,
                 kGreetInk, MiniCanvas::kCenter, MiniCanvas::kMiddle);
    }
    return;
  }

  const float bounce = sinf(fminf(1.0f, age / 0.7f) * kPi) * 3;
  c.strokeArc(82, 104 + bounce, 18, 4.0f, kPi * 1.1f, kPi * 1.9f, kCream);
  c.strokeArc(158, 104 - bounce, 18, 4.0f, kPi * 1.1f, kPi * 1.9f, kCream);
  fillArc(c, 120, 126, 30, 0, kPi, kCream);
  c.fillEllipse(120, 147, 14, 8 + 2 * fabsf(sinf(T * 8)), kCoral);
  c.setAlpha(0.62f);
  c.fillEllipse(54, 143, 14, 8, kCoral);
  c.fillEllipse(186, 143, 14, 8, kCoral);
  c.setAlpha(1.0f);
  heart(c, 120, 57 + bounce * 0.3f, done ? 15 + 2 * sinf(T * 6) : 13 + 2 * sinf(T * 6),
        done ? kCoral : kLoveRose);
  for (int i = 0; i < 6; ++i) {
    const float x = 34 + i * 34;
    const float y = 68 + fmodf(T * 24 + i * 23, 45);
    c.save(); c.translate(x,y); c.rotate(T*4+i);
    c.fillRect(-4,-2,8,4,i%3==0?kCoral:(i%3==1?kLime:kCyan)); c.restore();
  }
  if (wait) {
    const float p = clamp01(progress);
    const float a = -kPi / 2 + kTwoPi * p;
    const float r = 114;
    c.setAlpha(0.14f);
    c.strokeArc(120, 120, r, 5, 0, kTwoPi, kWhite);
    c.setAlpha(1.0f);
    if (p >= 0.999f) {
      c.strokeArc(120, 120, r, 5, 0, kTwoPi, kCyan);
    } else {
      c.strokeArc(120, 120, r, 5, -kPi / 2, a, kCyan);
    }
    c.fillEllipse(120 + r * cosf(a), 120 + r * sinf(a), 6, 6, kCyan);
    float fs=18;while(fs>14&&c.measureText(cap,fs)>214)--fs;
    c.fillText(120, 181, cap, fs, kGreetInk, MiniCanvas::kCenter, MiniCanvas::kMiddle);
    c.fillText(120, 207, "轻拍回复", 17, kGreetInk, MiniCanvas::kCenter,
               MiniCanvas::kMiddle);
  } else {
    float fs=19;while(fs>14&&c.measureText(cap,fs)>214)--fs;
    c.fillText(120, 204, cap, fs, kGreetInk, MiniCanvas::kCenter, MiniCanvas::kMiddle);
  }
}

// ---------- 朋友消息界面：头像 + 浅色气泡 ----------
#include "face_views.inc"
}  // namespace

bool FaceAnimation::begin() {
  if (frame_ != nullptr) return true;
  frame_ = static_cast<uint8_t*>(
      heap_caps_malloc(kBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frame_ == nullptr) {
    frame_ = static_cast<uint8_t*>(heap_caps_malloc(kBytes, MALLOC_CAP_8BIT));
  }
  if (frame_ == nullptr) {
    Serial.println("[FACE] framebuffer allocation failed");
    return false;
  }
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  lv_display_t* display = lv_display_create(240, 240);
  if (!display) return false;
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  static uint8_t scratch[240 * 8 * 2];
  lv_display_set_buffers(display, scratch, nullptr, sizeof(scratch), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
  canvas_ = MiniCanvas(frame_);
  if (!canvas_.init()) { Serial.println("[FACE] LVGL canvas allocation failed"); return false; }
  faceFrameBind(frame_);
  FaceUpdate initial{};
  strcpy(initial.face, "idle");
  render(initial, 0.0f, 0.0f, false);
  pushFull();
  Serial.printf("[FACE] idle face ready, flush=%lu us\n",
                static_cast<unsigned long>(lastFlushUs_));
  return true;
}

void FaceAnimation::render(const FaceUpdate& request, float time, float age, bool blink) {
  canvas_.begin(kBg);
  struct Finish { MiniCanvas& canvas; ~Finish(){canvas.finish();} } finish{canvas_};
  canvas_.save();
  const char* face = request.face;
  if (!strcmp(face, "clock")) {
    drawClockFace(canvas_, time, blink, request);
    canvas_.restore();
    return;
  }
  // The page paints these faces with their own helpers and their own ring.
  if (!strcmp(face, "chat")) {
    drawChat(canvas_, request, time, age);
    canvas_.strokeArc(120, 120, 114, 5, 0, kTwoPi, kBlue);
    canvas_.restore();
    return;
  }
  if (!strcmp(face, "stick")) {
    drawStick(canvas_, request, age);
    canvas_.strokeArc(120, 120, 114, 3, 0, kTwoPi, kWhite);
    canvas_.restore();
    return;
  }
  if (!strcmp(face, "gsend") || !strcmp(face, "grecv") ||
      !strcmp(face, "replyWait") || !strcmp(face, "replyDone")) {
    drawGreet(canvas_, face, request.text, time, age, request.holdProgress);
    if (strcmp(face, "replyWait") != 0) {
      canvas_.strokeArc(120, 120, 114, 5, 0, kTwoPi, glowColor(face));
    }
    canvas_.restore();
    return;
  }

  const float scale = request.text[0] ? 0.8f : 1.0f;
  if (request.text[0]) {
    canvas_.translate(120, 106);
    canvas_.scale(scale);
    canvas_.translate(-120, -112);
  }
  drawFace(canvas_, request.face, request.text, time, age, blink);

  const float k = blushFactor(request.face);
  const float radius = fminf(k, 1.3f);
  canvas_.setAlpha(fminf(0.95f, 0.55f * k));
  canvas_.fillEllipse(50, 148, 20 * radius, 11 * radius, kCoral);
  canvas_.fillEllipse(190, 148, 20 * radius, 11 * radius, kCoral);
  canvas_.setAlpha(1.0f);
  canvas_.restore();

  const bool offlineIdle = !strcmp(request.face,"idle") && !request.networkConnected;
  const uint32_t glow = offlineIdle ? kRed : glowColor(request.face);
  if (request.text[0]) {
    const float a = clamp01(age / 0.2f);
    const float pop = 1.0f + 0.22f * fmaxf(0.0f, 1.0f - age / 0.3f);
    canvas_.save();
    canvas_.setAlpha(a);
    canvas_.translate(120, 186 + (1.0f - a) * 12);
    canvas_.scale(pop);
    float fontSize=26; while(fontSize>14 && canvas_.measureText(request.text,fontSize)>160)--fontSize;
    canvas_.fillText(0, 0, request.text, fontSize, glow, MiniCanvas::kCenter,
                     MiniCanvas::kMiddle);
    canvas_.restore();
  }
  if(offlineIdle) canvas_.setAlpha(.20f+.80f*(.5f-.5f*cosf(time*kTwoPi/3.0f)));
  canvas_.strokeArc(120, 120, 114, 5, 0, kTwoPi, glow);
  canvas_.setAlpha(1);
  // Long-press progress ring: track, filled arc and the moving knob.
  if (request.hold && request.holdProgress > 0.0f) {
    const float p = clamp01(request.holdProgress);
    const float a = -kPi / 2 + kTwoPi * p;
    canvas_.setAlpha(0.2f);
    canvas_.strokeArc(120, 120, 113, 6, 0, kTwoPi, kWhite);
    canvas_.setAlpha(1.0f);
    if (p >= 0.999f) {
      canvas_.strokeArc(120, 120, 113, 6, 0, kTwoPi, kWhite);
    } else {
      canvas_.strokeArc(120, 120, 113, 6, -kPi / 2, a, kLime);
    }
    canvas_.fillEllipse(120 + 113 * cosf(a), 120 + 113 * sinf(a), 7, 7, kCoral);
  }
}

void FaceAnimation::update(uint32_t nowMs, const FaceUpdate& request) {
  if (!frame_) return;
  if (nowMs - lastRenderMs_ < 33) return;
  FaceUpdate req = request;
  req.networkConnected = deviceState.network();
  const bool connected = req.seq != 0;
  if (!connected) { strcpy(req.face, "idle"); strcpy(req.stick, "idle"); }
  const float elapsed = connected && !req.paused ? (nowMs - req.updatedMs) / 1000.0f : 0;
  const float time = connected ? req.time + elapsed : nowMs / 1000.0f;
  float age = connected ? req.age + elapsed : nowMs / 1000.0f;
  bool blinking = connected ? req.blink && (req.paused || elapsed < req.blinkRemaining) : nowMs % 3500 < 120;
  const auto local = deviceState.localFace();
  if(local.seq && int32_t(local.untilMs-nowMs)>0 && !req.paused){
    strcpy(req.face,"dizzy");req.text[0]=0;req.hold=false;req.holdRate=0;
    age=(nowMs-local.startedMs)/1000.0f;blinking=false;
  }
  req.holdProgress = clamp01(req.holdProgress + elapsed * req.holdRate);
  if (req.wallClockMs > 0) {
    double ms = req.wallClockMs + elapsed * 1000 + req.utcOffsetMinutes * 60000.0;
    time_t seconds = static_cast<time_t>(ms / 1000);
    struct tm fields{}; gmtime_r(&seconds, &fields);
    req.clockHour = fields.tm_hour; req.clockMinute = fields.tm_min; req.clockSecond = fields.tm_sec;
    req.clockSeconds = fields.tm_sec + fmod(ms, 1000) / 1000;
    req.clockMonth = fields.tm_mon + 1; req.clockDay = fields.tm_mday; req.clockWeekday = fields.tm_wday;
  }
  // Idle is static outside blinking. State changes (including rings) always redraw.
  const bool staticIdle = !strcmp(req.face, "idle") && !req.text[0] && !req.hold && req.networkConnected;
  static bool previousNetwork=false;static char previousFace[16]="";
  const bool sameView=previousNetwork==req.networkConnected && !strcmp(previousFace,req.face);
  if (sameView && renderedSeq_ == req.seq && blinking == blinkRendered_ && (staticIdle || req.paused)) return;
  const uint32_t start = micros();
  faceFrameNextSeq(req.clientSeq);
  render(req, time, age, blinking);
  const uint32_t renderUs = micros() - start;
  pushFull();
  faceRequest.displayed(req.clientSeq, renderUs, lastFlushUs_);
  renderedSeq_ = req.seq; lastRenderMs_ = nowMs;
  previousNetwork=req.networkConnected;strcpy(previousFace,req.face);
  if (blinking && !blinkRendered_) ++blinks_;
  blinkRendered_ = blinking;
}

void FaceAnimation::pushFull() {
  faceRenderStage=5;
  LCD_1IN28_SetWindows(0, 0, MiniCanvas::kWidth, MiniCanvas::kHeight);
  DEV_Digital_Write(LCD_DC_PIN, HIGH);
  const uint32_t started = micros();
  DEV_SPI_Write_nByte(frame_, kBytes);
  lastFlushUs_ = micros() - started;
}
