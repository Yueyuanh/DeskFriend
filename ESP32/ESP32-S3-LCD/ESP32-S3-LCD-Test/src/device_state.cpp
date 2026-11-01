#include "device_state.h"

DeviceState deviceState;
void DeviceState::network(bool connected) { portENTER_CRITICAL(&mux_);connected_=connected;portEXIT_CRITICAL(&mux_); }
bool DeviceState::network() { portENTER_CRITICAL(&mux_);bool result=connected_;portEXIT_CRITICAL(&mux_);return result; }
void DeviceState::shake(uint32_t nowMs) { portENTER_CRITICAL(&mux_);++local_.seq;local_.startedMs=nowMs;local_.untilMs=nowMs+3000;portEXIT_CRITICAL(&mux_); }
LocalFaceEvent DeviceState::localFace() { portENTER_CRITICAL(&mux_);auto result=local_;portEXIT_CRITICAL(&mux_);return result; }

void DeviceState::updateImu(const ImuSnapshot& value) {
  portENTER_CRITICAL(&mux_);
  imu_ = value;
  portEXIT_CRITICAL(&mux_);
}

ImuSnapshot DeviceState::imu() {
  portENTER_CRITICAL(&mux_);
  const ImuSnapshot result = imu_;
  portEXIT_CRITICAL(&mux_);
  return result;
}
