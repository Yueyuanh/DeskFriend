#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class WebTask_t {
 public:
  void begin();
  void run(void* parameters);
  static void runTask(void* parameters);
};
extern WebTask_t webTaskInstance;
extern TaskHandle_t webTaskHandle;
