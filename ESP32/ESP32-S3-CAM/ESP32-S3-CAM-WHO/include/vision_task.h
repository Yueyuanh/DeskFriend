#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_err.h"

// Camera capture + pedestrian/face inference pipeline.
// The task blocks in esp_camera_fb_get() and only yields 1 tick per frame,
// so it is pinned to core 1 and kept above the web task.
class VisionTask_t {
public:
    static constexpr const char *kTaskName = "Vision Task";
    static constexpr uint32_t kStackSize = 12288;
    static constexpr UBaseType_t kPriority = 5;
    static constexpr BaseType_t kCoreId = 1;

    // Initializes the camera driver and publishes the boot message.
    // Called from app_main() before the task is created.
    esp_err_t begin();

    // Task body. Never returns; failures are reported through app_state and
    // the loop backs off instead of spinning.
    void run(void *pvParameters);
    static void runTask(void *pvParameters);
};

extern VisionTask_t visionTaskInstance;
extern TaskHandle_t visionTaskHandle;
