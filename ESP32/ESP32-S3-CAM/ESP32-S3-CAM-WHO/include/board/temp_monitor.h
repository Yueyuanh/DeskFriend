#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

// Chip temperature sampled every TEMP_SAMPLE_MS and kept in a small ring
// buffer for the web chart. The OV5640 module has no on-board temperature
// sensor, so there is no camera reading to pair with it.
constexpr uint32_t TEMP_SAMPLE_MS = 2000;
constexpr size_t TEMP_HISTORY_SIZE = 90;  // 3 minutes at 2 s

esp_err_t temp_monitor_start();

// Latest chip temperature in degrees Celsius. Returns false until the first
// sample has been taken.
bool temp_monitor_latest(float *out_celsius);

// Copies the history oldest -> newest. Returns the number of samples written.
size_t temp_monitor_history(float *out, size_t max);
