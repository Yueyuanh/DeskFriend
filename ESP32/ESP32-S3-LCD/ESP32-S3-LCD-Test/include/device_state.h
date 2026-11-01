#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct ImuSnapshot {
  bool ready;
  float acceleration[3];
  float gyro[3];
  float angles[3];
  uint32_t updatedMs;
};

struct LocalFaceEvent { uint32_t seq=0, startedMs=0, untilMs=0; };
// Small cross-task state shared by the display task (writer) and HTTP task (reader).
// The display no longer accepts frames from the network; the browser only reads this.
class DeviceState {
 public:
  void updateImu(const ImuSnapshot& value);
  ImuSnapshot imu();
  void network(bool connected);
  bool network();
  void shake(uint32_t nowMs);
  LocalFaceEvent localFace();

 private:
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  ImuSnapshot imu_ = {};
  bool connected_=false;
  LocalFaceEvent local_{};
};

extern DeviceState deviceState;
