#include <Arduino.h>
#include "servo_task.h"
#include "led_task.h"
#include "web_task.h"

void setup() {
  Serial.begin(115200);

  servoTaskInstance.begin();
  ledTaskInstance.begin();

  xTaskCreatePinnedToCore(
      servoTaskInstance.runTask, "Servo Task", 3072, nullptr, 1,
      &servoTaskHandle, 1);
  xTaskCreatePinnedToCore(
      ledTaskInstance.runTask, "LED Task", 3072, nullptr, 1,
      &ledTaskHandle, 1);
  xTaskCreatePinnedToCore(
      webTaskInstance.runTask, "Web Task", 8192, nullptr, 1,
      &webTaskHandle, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(10));
}
