#include "display_task.h"

#include <math.h>

#include "DEV_Config.h"
#include "LCD_1in28.h"
#include "QMI8658.h"
#include "device_state.h"
#include "face_animation.h"
#include "face_state.h"
#include "face_frame.h"

namespace {
constexpr uint32_t kTaskPeriodMs = 5;
constexpr uint32_t kImuPeriodMs = 25;
constexpr uint32_t kStatusPeriodMs = 5000;

FaceAnimation face;
bool imuReady = false;

void readImu(uint32_t now, uint32_t& previousMs, float& yawDegrees) {
  float acceleration[3] = {0.0f, 0.0f, 0.0f};
  float gyro[3] = {0.0f, 0.0f, 0.0f};
  QMI8658_read_xyz(acceleration, gyro, nullptr);

  // A change in acceleration rejects static gravity and ordinary orientation
  // changes. Require two impulses, then latch one 3 s animation with cooldown.
  static bool primed=false;
  static float previousAcceleration[3]{};
  static uint32_t previousImpulse=0,lastShake=0;
  float jerkSquared=0;
  for(int axis=0;axis<3;++axis){float change=acceleration[axis]-previousAcceleration[axis];jerkSquared+=change*change;previousAcceleration[axis]=acceleration[axis];}
  if(primed && jerkSquared>7.5f*7.5f && (!lastShake || now-lastShake>3800)){
    if(previousImpulse && now-previousImpulse<450){
      deviceState.shake(now);lastShake=now;previousImpulse=0;
      Serial.printf("[IMU] shake -> dizzy, duration=3000 ms, delta=%.2f m/s2\n",sqrtf(jerkSquared));
    }else previousImpulse=now;
  }
  primed=true;

  const float dt = (now - previousMs) / 1000.0f;
  previousMs = now;

  const float roll = atan2f(acceleration[1], acceleration[2]) * 57.29578f;
  const float pitch =
      atan2f(-acceleration[0],
             sqrtf(acceleration[1] * acceleration[1] + acceleration[2] * acceleration[2])) *
      57.29578f;
  yawDegrees += gyro[2] * 57.29578f * dt;
  if (yawDegrees > 180.0f) yawDegrees -= 360.0f;
  if (yawDegrees < -180.0f) yawDegrees += 360.0f;

  ImuSnapshot snapshot{};
  snapshot.ready = true;
  snapshot.updatedMs = now;
  snapshot.angles[0] = roll;
  snapshot.angles[1] = pitch;
  snapshot.angles[2] = yawDegrees;
  for (uint8_t axis = 0; axis < 3; ++axis) {
    snapshot.acceleration[axis] = acceleration[axis];
    snapshot.gyro[axis] = gyro[axis];
  }
  deviceState.updateImu(snapshot);
}
}  // namespace

DisplayTask_t displayTaskInstance;
TaskHandle_t displayTaskHandle = nullptr;

void DisplayTask_t::begin() {
  Serial.println("[DISPLAY] Waveshare LCD init");
  DEV_Module_Init();
  LCD_1IN28_Init(HORIZONTAL);
  imuReady = QMI8658_init() != 0;
  Serial.printf("[IMU] QMI8658 %s\n", imuReady ? "ready" : "not found");
  if (!face.begin()) {
    Serial.println("[DISPLAY] default face unavailable");
  }
}

void DisplayTask_t::run(void* parameters) {
  (void)parameters;
  begin();  // Hardware and the face buffer belong to this task.
  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastImuRead = millis();
  uint32_t lastStatusLog = lastImuRead;
  uint32_t previousImuMs = lastImuRead;
  float yawDegrees = 0.0f;

  for (;;) {
    const uint32_t now = millis();
    const FaceUpdate request = faceRequest.snapshot();
    face.update(now, request);
    if (imuReady && now - lastImuRead >= kImuPeriodMs) {
      lastImuRead = now;
      faceRenderStage=6;
      readImu(millis(), previousImuMs, yawDegrees);
    }
    if (now - lastStatusLog >= kStatusPeriodMs) {
      lastStatusLog = now;
      Serial.printf("[DISPLAY] face=%s, seq=%lu, heap=%lu, stack_free=%lu, blinks=%lu, "
                    "flush=%lu us\n",
                    request.face, static_cast<unsigned long>(request.seq),
                    static_cast<unsigned long>(ESP.getFreeHeap()),
                    static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)),
                    static_cast<unsigned long>(face.blinks()),
                    static_cast<unsigned long>(face.lastFlushUs()));
    }
    faceRenderStage=0;
    // A full vector frame can exceed 5 ms. Do not accumulate missed periods
    // and starve the idle task while trying to catch up.
    lastWake=xTaskGetTickCount();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(kTaskPeriodMs));
  }
}

void DisplayTask_t::runTask(void* parameters) { displayTaskInstance.run(parameters); }
