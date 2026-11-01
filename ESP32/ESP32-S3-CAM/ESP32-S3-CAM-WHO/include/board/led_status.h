#pragma once

#include "esp_err.h"

#include <cstdint>

// WS2812 status LED on the GOOUUU ESP32-S3-CAM (single pixel, GPIO48).
// Red blink 500 ms while Wi-Fi has no IP, solid green once connected.
// Same behaviour as ESP32-S3-Dual/src/led_task.cpp, driven by RMT instead of
// Adafruit_NeoPixel because this project is plain ESP-IDF.
esp_err_t led_status_start();

// Show one colour for `hold_ms` and then fall back to the Wi-Fi status colour.
// Channels are 0-255 and get scaled by the same dimming factor as the status
// colours, so the LED never gets brighter than normal operation. Used by the
// CDC `LED <r> <g> <b>` command to check the pixel and its channel order.
void led_status_flash(uint8_t r, uint8_t g, uint8_t b, uint32_t hold_ms);
