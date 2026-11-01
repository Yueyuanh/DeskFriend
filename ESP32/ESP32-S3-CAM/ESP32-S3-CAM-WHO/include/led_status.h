#pragma once

#include "esp_err.h"

// WS2812 status LED on the GOOUUU ESP32-S3-CAM (single pixel, GPIO48).
// Red blink 500 ms while Wi-Fi has no IP, solid green once connected.
// Same behaviour as ESP32-S3-Dual/src/led_task.cpp, driven by RMT instead of
// Adafruit_NeoPixel because this project is plain ESP-IDF.
esp_err_t led_status_start();
