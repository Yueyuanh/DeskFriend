#include "led_task.h"

namespace {
constexpr uint32_t STATUS_BLINK_MS = 500;
constexpr uint32_t LED_UPDATE_MS = 20;
} // namespace

LedTask_t ledTaskInstance;
TaskHandle_t ledTaskHandle = nullptr;
volatile bool wifiConnected = false;
volatile bool expressionColorEnabled = false;
volatile uint8_t expressionRed = 0;
volatile uint8_t expressionGreen = 0;
volatile uint8_t expressionBlue = 0;

void setExpressionColor(uint8_t red, uint8_t green, uint8_t blue) {
  expressionRed = red;
  expressionGreen = green;
  expressionBlue = blue;
  expressionColorEnabled = true;
}

void LedTask_t::begin() {
  pixel.begin();
  pixel.setBrightness(64);
  pixel.clear();
  pixel.show();
}

void LedTask_t::run(void *pvParameters) {
  (void)pvParameters;
  uint32_t lastStatusChange = millis();
  bool blinkOn = false;
  uint32_t lastColor = 0xffffffffUL;

  for (;;) {
    uint32_t color = 0;
    if (!wifiConnected) {
      if (millis() - lastStatusChange >= STATUS_BLINK_MS) {
        lastStatusChange = millis();
        blinkOn = !blinkOn;
      }
      color = blinkOn ? pixel.Color(64, 0, 0) : 0; // WiFi not connected: red blink.
    } else if (expressionColorEnabled) {
      color = pixel.Color(expressionRed, expressionGreen, expressionBlue);
    } else {
      color = pixel.Color(0, 64, 0); // Connected: green.
    }

    if (color != lastColor) {
      pixel.setPixelColor(0, color);
      pixel.show();
      lastColor = color;
    }
    vTaskDelay(pdMS_TO_TICKS(LED_UPDATE_MS));
  }
}

void LedTask_t::runTask(void *pvParameters) {
  ledTaskInstance.run(pvParameters);
}
