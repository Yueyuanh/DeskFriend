#include "face_frame.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
namespace { uint8_t* pixels=nullptr; SemaphoreHandle_t mutex=nullptr; uint32_t nextSeq=0,publishedSeq=0; }
volatile uint32_t faceRenderStage=0;
void faceFrameBind(uint8_t* frame) { mutex=xSemaphoreCreateMutex(); pixels=frame; }
void faceFrameLock() { if(mutex)xSemaphoreTake(mutex,portMAX_DELAY); }
void faceFrameUnlock() { if(mutex)xSemaphoreGive(mutex); }
void faceFrameNextSeq(uint32_t seq) { nextSeq=seq; }
void faceFramePublish() { publishedSeq=nextSeq; }
uint8_t* faceFrameCopy(uint32_t* seq) {
  if(!pixels || !mutex)return nullptr;
  auto* copy=static_cast<uint8_t*>(heap_caps_malloc(240*240*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!copy)return nullptr;
  faceFrameLock();memcpy(copy,pixels,240*240*2);*seq=publishedSeq;faceFrameUnlock();return copy;
}
