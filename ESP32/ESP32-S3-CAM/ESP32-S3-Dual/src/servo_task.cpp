#include "servo_task.h"
#include <math.h>

namespace {
constexpr uint8_t PITCH_PIN = 47;
constexpr uint8_t YAW_PIN = 21;
constexpr uint16_t MIN_PULSE_US = 500;
constexpr uint16_t MAX_PULSE_US = 2500;
constexpr uint32_t UPDATE_INTERVAL_MS = 20;
constexpr float SMOOTH_ALPHA = 0.16f;
} // namespace

ServoTask_t servoTaskInstance;
TaskHandle_t servoTaskHandle = nullptr;
volatile uint8_t servoPitchAngle = 90;
volatile uint8_t servoYawAngle = 90;

void ServoTask_t::begin() {
  servo.attach(PITCH_PIN, MIN_PULSE_US, MAX_PULSE_US);
  yawServo.attach(YAW_PIN, MIN_PULSE_US, MAX_PULSE_US);
  servo.write(servoPitchAngle);
  yawServo.write(servoYawAngle);
}

void ServoTask_t::run(void *pvParameters) {
  (void)pvParameters;
  float pitchCurrent = servoPitchAngle;
  float yawCurrent = servoYawAngle;
  uint8_t lastPitch = 255;
  uint8_t lastYaw = 255;

  for (;;) {
    const float pitchTarget = constrain(static_cast<int>(servoPitchAngle), 0, 180);
    const float yawTarget = constrain(static_cast<int>(servoYawAngle), 0, 180);
    pitchCurrent += (pitchTarget - pitchCurrent) * SMOOTH_ALPHA;
    yawCurrent += (yawTarget - yawCurrent) * SMOOTH_ALPHA;

    const uint8_t pitchOutput = static_cast<uint8_t>(roundf(pitchCurrent));
    const uint8_t yawOutput = static_cast<uint8_t>(roundf(yawCurrent));
    if (pitchOutput != lastPitch) {
      servo.write(pitchOutput);
      lastPitch = pitchOutput;
    }
    if (yawOutput != lastYaw) {
      yawServo.write(yawOutput);
      lastYaw = yawOutput;
    }
    vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
  }
}

void ServoTask_t::runTask(void *pvParameters) {
  servoTaskInstance.run(pvParameters);
}
