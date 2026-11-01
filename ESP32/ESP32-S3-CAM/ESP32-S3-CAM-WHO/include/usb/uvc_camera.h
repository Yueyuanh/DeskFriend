#pragma once

#include "esp_err.h"

// Exposes the shared camera frames as a USB Video Class (UVC) webcam on the
// board's OTG Type-C port. The Wi-Fi path keeps running; UVC only reads the
// newest JPEG that the vision task publishes, so both outputs stay in sync and
// no second owner of the camera driver is introduced.
esp_err_t uvc_camera_start();

// True while a USB host has opened the camera and is streaming.
bool uvc_camera_streaming();
