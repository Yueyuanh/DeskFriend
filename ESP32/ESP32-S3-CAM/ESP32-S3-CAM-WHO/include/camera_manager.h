#pragma once

#include "esp_err.h"

// Owns the single esp_camera instance. Called once from VisionTask_t::begin()
// before the vision task starts, so no other task touches the driver directly.
esp_err_t camera_manager_begin();

// Releases the camera driver. Currently unused, but keeps the ownership
// boundary explicit for recovery/restart paths.
esp_err_t camera_manager_end();
