#include "led_task.h"

LedTask_t ledTaskInstance;

LedTask_t::LedTask_t()
{
}

void LedTask_t::initPeripheral()
{

}

void LedTask_t::run(void *pvParameters)
{


    Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.setBrightness(10);  // 亮度 0-255
    strip.show();  // 初始化LED为关闭状态

    while (true)
    {
        // 红色
        strip.setPixelColor(0, strip.Color(255, 0, 0));
        strip.show();
        delay(1000);
        
        // 绿色
        strip.setPixelColor(0, strip.Color(0, 255, 0));
        strip.show();
        delay(1000);

        // 蓝色
        strip.setPixelColor(0, strip.Color(0, 0, 255));
        strip.show();
        delay(1000);
    }
}

void LedTask_t::runTask(void *pvParameters)
{
    ledTaskInstance.run(pvParameters);
}

