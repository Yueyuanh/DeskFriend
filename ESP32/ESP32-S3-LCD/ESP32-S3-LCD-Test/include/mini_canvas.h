#pragma once
#include <lvgl.h>
#include <stdint.h>

// Canvas2D adapter backed by LVGL 9 / ThorVG. Compose RGB888, then quantize once.
class MiniCanvas {
 public:
  static constexpr int kWidth = 240, kHeight = 240, kBytes = kWidth * kHeight * 2;
  enum Align { kLeft, kCenter, kRight };
  enum Baseline { kAlphabetic, kMiddle };
  explicit MiniCanvas(uint8_t* framebuffer = nullptr) : frame_(framebuffer) {}
  bool init();
  void begin(uint32_t background);
  void finish();
  void setAlpha(float alpha);
  void save(); void restore();
  void translate(float x, float y); void scale(float factor); void rotate(float radians);
  void fillEllipse(float x, float y, float rx, float ry, uint32_t color);
  void strokeEllipse(float x, float y, float rx, float ry, float halfWidth, uint32_t color);
  void strokeArc(float x, float y, float r, float halfWidth, float start, float end, uint32_t color, bool roundCaps = true);
  void fillArc(float x, float y, float r, float start, float end, uint32_t color);
  void fillHalfEllipse(float x,float y,float rx,float ry,uint32_t color);
  void strokeLine(float x0, float y0, float x1, float y1, float halfWidth, uint32_t color);
  void fillRect(float x, float y, float w, float h, uint32_t color);
  void strokeRect(float x, float y, float w, float h, float halfWidth, uint32_t color);
  void fillRoundRect(float x, float y, float w, float h, float radius, uint32_t color);
  void fillPolygon(const float* xs, const float* ys, int count, uint32_t color);
  void strokePolyline(const float* xs, const float* ys, int count, float halfWidth, uint32_t color);
  void fillHeart(float x, float y, float s, uint32_t color);
  void fillDrop(float x, float y, float s, uint32_t color);
  void strokeQuadratics(float x0,float y0,float cx,float cy,float x1,float y1,float cx2,float cy2,float x2,float y2,float w,uint32_t color);
  void strokeCubic(float x0,float y0,float c1x,float c1y,float c2x,float c2y,float x1,float y1,float w,uint32_t color);
  static int flattenQuadratic(float x0,float y0,float cx,float cy,float x1,float y1,float* xs,float* ys,int capacity);
  static int flattenCubic(float x0,float y0,float c1x,float c1y,float c2x,float c2y,float x1,float y1,float* xs,float* ys,int capacity);
  float measureText(const char* text,float size,bool regular=false) const;
  void fillText(float x,float y,const char* text,float size,uint32_t color,Align align=kLeft,Baseline baseline=kAlphabetic,bool regular=false);
  void pixelGrid(const uint8_t* grid);
 private:
  struct State { lv_matrix_t matrix; float alpha; } state_{}, stack_[12]{};
  void submit(uint32_t color,float halfWidth=0,bool fill=true,bool round=true);
  void arcPath(float x,float y,float r,float start,float end);
  void flushVectors(); void resetPath();
  static lv_font_t* fontFor(int size,bool regular);
  uint8_t* frame_=nullptr;
  uint32_t* rgba_=nullptr;
  lv_obj_t* canvas_=nullptr;
  lv_layer_t layer_{};
  lv_draw_vector_dsc_t* vector_=nullptr;
  lv_vector_path_t* path_=nullptr;
  int depth_=0;
  bool pending_=false;
};
