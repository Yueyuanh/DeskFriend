#pragma once

#include "esp_camera.h"
#include "esp_err.h"

// Owns the single esp_camera instance. Called once from VisionTask_t::begin()
// before the vision task starts, so no other task touches the driver directly.
esp_err_t camera_manager_begin();

// Reconfigures the sensor for a new frame size by deinit + init. Must only be
// called from the vision task, between frames, so no frame buffer is in flight.
// Falls back to QVGA when the requested mode cannot be initialized.
esp_err_t camera_manager_set_framesize(framesize_t frame_size);

// Releases the camera driver. Currently unused, but keeps the ownership
// boundary explicit for recovery/restart paths.
esp_err_t camera_manager_end();
