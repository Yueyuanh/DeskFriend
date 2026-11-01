#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display_task.h"
#include "web_task.h"

void setup() {
  Serial.begin(115200);
  // The physical board uses UART0; the borrowed touch board's USB CDC is disabled.
  delay(500);
  Serial.println("[LCD] DeskFriend LVGL state sync / IMU shake / Wi-Fi");
  Serial.println("[LCD] Touch input is intentionally not implemented.");

  Serial.printf("[BOOT] flash=%lu PSRAM=%lu\n",
                static_cast<unsigned long>(ESP.getFlashChipSize()),
                static_cast<unsigned long>(ESP.getPsramSize()));
  webTaskInstance.begin();
  const BaseType_t display = xTaskCreatePinnedToCore(displayTaskInstance.runTask,
      "Display Task", 49152, nullptr, 1, &displayTaskHandle, 1);
  const BaseType_t web = xTaskCreatePinnedToCore(webTaskInstance.runTask,
      "Web Task", 6144, nullptr, 1, &webTaskHandle, 0);
  Serial.printf("[BOOT] display task=%s, web task=%s\n",
                display == pdPASS ? "OK" : "FAILED", web == pdPASS ? "OK" : "FAILED");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(10));
}
