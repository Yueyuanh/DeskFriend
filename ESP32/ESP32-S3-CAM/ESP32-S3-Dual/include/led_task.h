#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class LedTask_t {
public:
  void begin();
  void run(void *pvParameters);
  static void runTask(void *pvParameters);

private:
  Adafruit_NeoPixel pixel{1, 48, NEO_GRB + NEO_KHZ800};
};

extern LedTask_t ledTaskInstance;
extern TaskHandle_t ledTaskHandle;
extern volatile bool wifiConnected;
extern volatile bool expressionColorEnabled;
extern volatile uint8_t expressionRed;
extern volatile uint8_t expressionGreen;
extern volatile uint8_t expressionBlue;

void setExpressionColor(uint8_t red, uint8_t green, uint8_t blue);
