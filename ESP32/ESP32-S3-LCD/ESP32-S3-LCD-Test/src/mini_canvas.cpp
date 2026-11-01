#include "mini_canvas.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include "face_fonts.h"
#include "face_frame.h"
#include <Arduino.h>
#include <algorithm>

extern "C" float desk_face_ttf_advance(const lv_font_t*,uint32_t,uint32_t);

namespace {
constexpr float pi=3.14159265358979323846f;
struct FontSlot { int size=0; bool regular=false; lv_font_t* font=nullptr; uint32_t used=0; };
FontSlot fonts[12]; uint32_t fontClock=0;
lv_fpoint_t pt(float x,float y) { return {x,y}; }
uint32_t nextCode(const char*& s) {
  uint32_t c=static_cast<unsigned char>(*s++);
  if(c<128)return c;
  int n=c<224?1:c<240?2:3;c&=(1u<<(6-n))-1;
  while(n-- && *s)c=(c<<6)|(static_cast<unsigned char>(*s++)&63);
  return c;
}
float textWidth(const lv_font_t* font,const char* text) {
  float width=0;
  while(*text){uint32_t code=nextCode(text);const char* peek=text;uint32_t next=*peek?nextCode(peek):0;width+=desk_face_ttf_advance(font,code,next);}
  return width;
}
}
bool MiniCanvas::init() {
  rgba_=static_cast<uint32_t*>(heap_caps_malloc(kWidth*kHeight*4,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!rgba_)return false;
  canvas_=lv_canvas_create(lv_screen_active());
  lv_canvas_set_buffer(canvas_,rgba_,kWidth,kHeight,LV_COLOR_FORMAT_ARGB8888);
  lv_obj_add_flag(canvas_,LV_OBJ_FLAG_HIDDEN);
  path_=lv_vector_path_create(LV_VECTOR_PATH_QUALITY_HIGH);
  return path_!=nullptr;
}
void MiniCanvas::begin(uint32_t background) {
  faceRenderStage=1;
  depth_=0;lv_matrix_identity(&state_.matrix);state_.alpha=1;
  for(int i=0;i<kWidth*kHeight;++i)rgba_[i]=0xff000000|background;
  lv_canvas_init_layer(canvas_,&layer_);vector_=lv_draw_vector_dsc_create(&layer_);pending_=false;
}
void MiniCanvas::finish() {
  faceRenderStage=3;
  const uint32_t started=micros();
  flushVectors();lv_draw_vector_dsc_delete(vector_);vector_=nullptr;
  lv_canvas_finish_layer(canvas_,&layer_);
  const uint32_t vectorUs=micros()-started;
  faceRenderStage=4;
  faceFrameLock();
  // Clip once per scanline and write whole 16-bit pixels. The former floating
  // point circle test and two PSRAM byte stores per pixel took ~50 ms/frame.
  for(int y=0;y<kHeight;++y){
    const int dy=2*y+1-240;
    int left=0;
    while((2*left+1-240)*(2*left+1-240)+dy*dy>240*240)++left;
    const int right=kWidth-left;
    auto* dst=reinterpret_cast<uint16_t*>(frame_)+y*kWidth;
    memset(dst,0,left*2);memset(dst+right,0,left*2);
    const uint32_t* src=rgba_+y*kWidth+left;
    dst+=left;
    for(int x=left;x<right;++x){
      const uint32_t rgb=*src++;
      const uint16_t v=((rgb>>8)&0xf800)|((rgb>>5)&0x07e0)|((rgb>>3)&0x001f);
      *dst++=__builtin_bswap16(v);
    }
  }
  faceFramePublish();
  faceFrameUnlock();
  static uint32_t lastProfile=0;
  if(millis()-lastProfile>5000){lastProfile=millis();Serial.printf("[LVGL] finish vectors=%lu convert=%lu us\n",(unsigned long)vectorUs,(unsigned long)(micros()-started-vectorUs));}
}
void MiniCanvas::setAlpha(float a){state_.alpha=fmaxf(0,fminf(1,a));}
void MiniCanvas::save(){if(depth_<12)stack_[depth_++]=state_;}
void MiniCanvas::restore(){if(depth_)state_=stack_[--depth_];}
void MiniCanvas::translate(float x,float y){lv_matrix_translate(&state_.matrix,x,y);}
void MiniCanvas::scale(float s){lv_matrix_scale(&state_.matrix,s,s);}
void MiniCanvas::rotate(float a){lv_matrix_rotate(&state_.matrix,a*180/pi);}
void MiniCanvas::resetPath(){lv_vector_path_clear(path_);}
void MiniCanvas::submit(uint32_t color,float hw,bool fill,bool round){
  lv_draw_vector_dsc_set_transform(vector_,&state_.matrix);
  lv_draw_vector_dsc_set_fill_color(vector_,lv_color_hex(color));
  lv_draw_vector_dsc_set_fill_opa(vector_,fill?lroundf(state_.alpha*255):0);
  lv_draw_vector_dsc_set_stroke_color(vector_,lv_color_hex(color));
  lv_draw_vector_dsc_set_stroke_opa(vector_,fill?0:lroundf(state_.alpha*255));
  lv_draw_vector_dsc_set_stroke_width(vector_,hw*2);
  lv_draw_vector_dsc_set_stroke_cap(vector_,round?LV_VECTOR_STROKE_CAP_ROUND:LV_VECTOR_STROKE_CAP_BUTT);
  lv_draw_vector_dsc_set_stroke_join(vector_,LV_VECTOR_STROKE_JOIN_ROUND);
  lv_draw_vector_dsc_add_path(vector_,path_);pending_=true;
}
void MiniCanvas::flushVectors(){
  if(!pending_)return;
  faceRenderStage=2;
  lv_draw_vector(vector_);lv_canvas_finish_layer(canvas_,&layer_);lv_canvas_init_layer(canvas_,&layer_);pending_=false;
}
void MiniCanvas::fillEllipse(float x,float y,float rx,float ry,uint32_t color){resetPath();auto p=pt(x,y);lv_vector_path_append_circle(path_,&p,rx,ry);submit(color);}
void MiniCanvas::strokeEllipse(float x,float y,float rx,float ry,float w,uint32_t color){resetPath();auto p=pt(x,y);lv_vector_path_append_circle(path_,&p,rx,ry);submit(color,w,false);}
void MiniCanvas::strokeArc(float x,float y,float r,float w,float a,float b,uint32_t color,bool round){
  if(fabsf(b-a)<.00001f)return;
  resetPath();arcPath(x,y,r,a,b);submit(color,w,false,round);
}
void MiniCanvas::arcPath(float x,float y,float r,float a,float b){
  const float sweep=fminf(2*pi,fmaxf(0,b-a));
  if(sweep>=2*pi-.00001f){auto p=pt(x,y);lv_vector_path_append_circle(path_,&p,r,r);return;}
  // Equal segments avoid LVGL 9.4 append_arc's near-zero final segment at
  // exactly 90 degrees. Its 0/0 control point can hang ThorVG (offline face).
  const int count=std::max(1,int(ceilf(sweep/(pi/2))));
  const float step=sweep/count,k=4.0f/3*tanf(step/4);
  auto start=pt(x+r*cosf(a),y+r*sinf(a));lv_vector_path_move_to(path_,&start);
  for(int i=0;i<count;++i){
    float end=a+step;auto c1=pt(x+r*(cosf(a)-k*sinf(a)),y+r*(sinf(a)+k*cosf(a)));
    auto c2=pt(x+r*(cosf(end)+k*sinf(end)),y+r*(sinf(end)-k*cosf(end)));
    auto p=pt(x+r*cosf(end),y+r*sinf(end));lv_vector_path_cubic_to(path_,&c1,&c2,&p);a=end;
  }
}
void MiniCanvas::fillArc(float x,float y,float r,float a,float b,uint32_t color){resetPath();arcPath(x,y,r,a,b);lv_vector_path_close(path_);submit(color);}
void MiniCanvas::fillHalfEllipse(float x,float y,float rx,float ry,uint32_t color){
  resetPath();const float k=.55228475f;auto a=pt(x+rx,y),b=pt(x+rx,y+k*ry),c=pt(x+k*rx,y+ry),d=pt(x,y+ry);
  lv_vector_path_move_to(path_,&a);lv_vector_path_cubic_to(path_,&b,&c,&d);
  b=pt(x-k*rx,y+ry);c=pt(x-rx,y+k*ry);d=pt(x-rx,y);lv_vector_path_cubic_to(path_,&b,&c,&d);lv_vector_path_close(path_);submit(color);
}
void MiniCanvas::strokeLine(float x0,float y0,float x1,float y1,float w,uint32_t color){resetPath();auto a=pt(x0,y0),b=pt(x1,y1);lv_vector_path_move_to(path_,&a);lv_vector_path_line_to(path_,&b);submit(color,w,false);}
void MiniCanvas::fillRect(float x,float y,float w,float h,uint32_t color){fillRoundRect(x,y,w,h,0,color);}
void MiniCanvas::fillRoundRect(float x,float y,float w,float h,float r,uint32_t color){resetPath();lv_vector_path_append_rectangle(path_,x,y,w,h,r,r);submit(color);}
void MiniCanvas::strokeRect(float x,float y,float w,float h,float hw,uint32_t color){resetPath();lv_vector_path_append_rectangle(path_,x,y,w,h,0,0);submit(color,hw,false);}
void MiniCanvas::fillPolygon(const float* xs,const float* ys,int count,uint32_t color){resetPath();for(int i=0;i<count;++i){auto p=pt(xs[i],ys[i]);if(i)lv_vector_path_line_to(path_,&p);else lv_vector_path_move_to(path_,&p);}lv_vector_path_close(path_);submit(color);}
void MiniCanvas::strokePolyline(const float* xs,const float* ys,int count,float w,uint32_t color){resetPath();for(int i=0;i<count;++i){auto p=pt(xs[i],ys[i]);if(i)lv_vector_path_line_to(path_,&p);else lv_vector_path_move_to(path_,&p);}submit(color,w,false);}
void MiniCanvas::fillHeart(float x,float y,float s,uint32_t color){
  resetPath();auto a=pt(x,y+s*.35f),b=pt(x-s,y-s*.3f),c=pt(x-s*.5f,y-s),d=pt(x,y-s*.45f);
  lv_vector_path_move_to(path_,&a);lv_vector_path_cubic_to(path_,&b,&c,&d);b=pt(x+s*.5f,y-s);c=pt(x+s,y-s*.3f);lv_vector_path_cubic_to(path_,&b,&c,&a);lv_vector_path_close(path_);submit(color);
}
void MiniCanvas::fillDrop(float x,float y,float s,uint32_t color){
  resetPath();auto a=pt(x,y-s*1.4f),b=pt(x+s*.4f,y-s*.5f),c=pt(x+s,y),d=pt(x+s,y+s*.45f);
  lv_vector_path_move_to(path_,&a);lv_vector_path_cubic_to(path_,&b,&c,&d);const float k=.55228475f;
  b=pt(x+s,y+s*.45f+k*s);c=pt(x+k*s,y+s*1.45f);d=pt(x,y+s*1.45f);lv_vector_path_cubic_to(path_,&b,&c,&d);
  b=pt(x-k*s,y+s*1.45f);c=pt(x-s,y+s*.45f+k*s);d=pt(x-s,y+s*.45f);lv_vector_path_cubic_to(path_,&b,&c,&d);
  b=pt(x-s,y);c=pt(x-s*.4f,y-s*.5f);lv_vector_path_cubic_to(path_,&b,&c,&a);lv_vector_path_close(path_);submit(color);
}
void MiniCanvas::strokeQuadratics(float x0,float y0,float cx,float cy,float x1,float y1,float cx2,float cy2,float x2,float y2,float w,uint32_t color){resetPath();auto a=pt(x0,y0),b=pt(cx,cy),c=pt(x1,y1);lv_vector_path_move_to(path_,&a);lv_vector_path_quad_to(path_,&b,&c);b=pt(cx2,cy2);c=pt(x2,y2);lv_vector_path_quad_to(path_,&b,&c);submit(color,w,false);}
void MiniCanvas::strokeCubic(float x0,float y0,float c1x,float c1y,float c2x,float c2y,float x1,float y1,float w,uint32_t color){resetPath();auto a=pt(x0,y0),b=pt(c1x,c1y),c=pt(c2x,c2y),d=pt(x1,y1);lv_vector_path_move_to(path_,&a);lv_vector_path_cubic_to(path_,&b,&c,&d);submit(color,w,false);}
int MiniCanvas::flattenQuadratic(float x0,float y0,float cx,float cy,float x1,float y1,float* xs,float* ys,int capacity){int n=capacity<33?capacity:33;for(int i=0;i<n;++i){float t=float(i)/(n-1),u=1-t;xs[i]=u*u*x0+2*u*t*cx+t*t*x1;ys[i]=u*u*y0+2*u*t*cy+t*t*y1;}return n;}
int MiniCanvas::flattenCubic(float x0,float y0,float c1x,float c1y,float c2x,float c2y,float x1,float y1,float* xs,float* ys,int capacity){int n=capacity<49?capacity:49;for(int i=0;i<n;++i){float t=float(i)/(n-1),u=1-t;xs[i]=u*u*u*x0+3*u*u*t*c1x+3*u*t*t*c2x+t*t*t*x1;ys[i]=u*u*u*y0+3*u*u*t*c1y+3*u*t*t*c2y+t*t*t*y1;}return n;}
lv_font_t* MiniCanvas::fontFor(int size,bool regular){
  if(size<8)size=8;
  if(size>80)size=80;
  FontSlot* slot=&fonts[0];
  for(auto& f:fonts){if(f.font&&f.size==size&&f.regular==regular){f.used=++fontClock;return f.font;}if(!f.font||f.used<slot->used)slot=&f;}
  if(slot->font)lv_tiny_ttf_destroy(slot->font);
  const unsigned char* data=size==52?face_font_clock:(regular?face_font_regular:face_font_bold);
  size_t bytes=size==52?face_font_clock_size:(regular?face_font_regular_size:face_font_bold_size);
  slot->font=lv_tiny_ttf_create_data_ex(data,bytes,size,LV_FONT_KERNING_NORMAL,32);slot->size=size;slot->regular=regular;slot->used=++fontClock;return slot->font;
}
float MiniCanvas::measureText(const char* text,float size,bool regular)const{auto font=fontFor(lroundf(size),regular);return font?textWidth(font,text):0;}
void MiniCanvas::fillText(float x,float y,const char* text,float size,uint32_t color,Align align,Baseline baseline,bool regular){
  flushVectors();float scale=hypotf(state_.matrix.m[0][0],state_.matrix.m[1][0]);auto font=fontFor(lroundf(size*scale),regular);if(!font)return;
  const float width=textWidth(font,text);
  lv_fpoint_t p=pt(x,y);lv_matrix_transform_point(&state_.matrix,&p);
  // Canvas middle-baseline offsets measured with the embedded fonts in Chrome;
  // they differ from LVGL's line-box center. Recheck when replacing font assets.
  const float middle=lroundf(size*scale)==52?14.359375f/52:6.8125f/26;
  float baselineY=baseline==kAlphabetic?p.y:p.y+size*scale*middle;
  float cursor=p.x-(align==kCenter?width*.5f:align==kRight?width:0);
  int top=lroundf(baselineY-(font->line_height-font->base_line));
  while(*text){
    const char* start=text;uint32_t code=nextCode(text);const char* peek=text;uint32_t next=*peek?nextCode(peek):0;
    char glyph[5]{};memcpy(glyph,start,text-start);
    int left=lroundf(cursor);lv_area_t area={left,top,left+font->line_height*2,top+font->line_height+2};
    lv_draw_label_dsc_t dsc;lv_draw_label_dsc_init(&dsc);
    dsc.font=font;dsc.text=glyph;dsc.text_local=1;dsc.color=lv_color_hex(color);dsc.opa=lroundf(state_.alpha*255);dsc.flag=LV_TEXT_FLAG_EXPAND;lv_draw_label(&layer_,&dsc,&area);
    cursor+=desk_face_ttf_advance(font,code,next);
  }
  lv_canvas_finish_layer(canvas_,&layer_);lv_canvas_init_layer(canvas_,&layer_);
}
void MiniCanvas::pixelGrid(const uint8_t* grid){flushVectors();for(int y=0;y<60;++y)for(int x=0;x<60;++x)for(int dy=0;dy<4;++dy)for(int dx=0;dx<4;++dx){uint32_t v=grid[y*60+x];rgba_[(y*4+dy)*240+x*4+dx]=0xff000000|(v<<16)|(v<<8)|v;}}
