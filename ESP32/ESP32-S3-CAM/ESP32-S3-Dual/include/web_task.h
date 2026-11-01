#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

class WebTask_t {
public:
  void run(void *pvParameters);
  static void runTask(void *pvParameters);
};

extern WebTask_t webTaskInstance;
extern TaskHandle_t webTaskHandle;
