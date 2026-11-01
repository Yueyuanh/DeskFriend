#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     48  // GPIO48
#define LED_COUNT   1   // 1个LED


class LedTask_t
{
private:
    void initPeripheral();

public:
    LedTask_t();

    void run(void *pvParameters);
    static void runTask(void *pvParameters);
};

extern LedTask_t ledTaskInstance;