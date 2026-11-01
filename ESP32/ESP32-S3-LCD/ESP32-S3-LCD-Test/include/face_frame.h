#pragma once
#include <stdint.h>
// Diagnostics read a complete outgoing framebuffer. There is no frame upload.
void faceFrameBind(uint8_t* frame);
void faceFrameLock();
void faceFrameUnlock();
void faceFrameNextSeq(uint32_t seq);
void faceFramePublish(); // called while conversion holds the frame lock
uint8_t* faceFrameCopy(uint32_t* seq); // caller frees PSRAM allocation
extern volatile uint32_t faceRenderStage;
