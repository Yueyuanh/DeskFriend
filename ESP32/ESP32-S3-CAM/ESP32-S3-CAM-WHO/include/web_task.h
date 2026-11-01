#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// HTTP control/status server. esp_http_server creates its own worker task, so
// this task only registers the routes, starts the server and deletes itself.
class WebTask_t {
public:
    static constexpr const char *kTaskName = "Web Task";
    static constexpr uint32_t kStackSize = 8192;
    static constexpr UBaseType_t kPriority = 3;
    static constexpr BaseType_t kCoreId = 0;

    void run(void *pvParameters);
    static void runTask(void *pvParameters);
};

extern WebTask_t webTaskInstance;
extern TaskHandle_t webTaskHandle;
