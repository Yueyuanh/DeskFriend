#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class DisplayTask_t {
 public:
  void begin();
  void run(void* parameters);
  static void runTask(void* parameters);
};

extern DisplayTask_t displayTaskInstance;
extern TaskHandle_t displayTaskHandle;
