#include <Arduino.h>
#include <led_task.h> 
#include <cam_task.h>

TaskHandle_t ledTaskHandle;
TaskHandle_t CamTaskHandle;

void setup() {
  Serial.begin(115200);

  xTaskCreatePinnedToCore(ledTaskInstance.runTask, "Led Task", 2048, NULL, 1, &ledTaskHandle, 1);
  xTaskCreatePinnedToCore(camTaskInstance.runTask, "Cam Task", 4096, NULL, 1, &CamTaskHandle, 1);

}

void loop() {

}