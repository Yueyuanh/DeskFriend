#include "temp_monitor.h"

#include "driver/temperature_sensor.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {

constexpr char TAG[] = "deskfriend_temp";

temperature_sensor_handle_t s_sensor = nullptr;
SemaphoreHandle_t s_mutex = nullptr;

// Ring buffer, written by the sampling task and copied out for the web UI.
float s_history[TEMP_HISTORY_SIZE] = {};
size_t s_count = 0;  // valid samples, capped at TEMP_HISTORY_SIZE
size_t s_head = 0;   // next write position
float s_latest = 0.0f;
bool s_has_value = false;

void temp_task(void *)
{
    for (;;) {
        float celsius = 0.0f;
        if (s_sensor && temperature_sensor_get_celsius(s_sensor, &celsius) == ESP_OK) {
            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_latest = celsius;
                s_has_value = true;
                s_history[s_head] = celsius;
                s_head = (s_head + 1) % TEMP_HISTORY_SIZE;
                if (s_count < TEMP_HISTORY_SIZE) {
                    ++s_count;
                }
                xSemaphoreGive(s_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TEMP_SAMPLE_MS));
    }
}

} // namespace

esp_err_t temp_monitor_start()
{
    if (s_sensor) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }

    // ESP32-S3's internal sensor supports roughly -10..80 degrees Celsius.
    temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    ESP_RETURN_ON_ERROR(temperature_sensor_install(&config, &s_sensor), TAG, "install");
    ESP_RETURN_ON_ERROR(temperature_sensor_enable(s_sensor), TAG, "enable");

    if (xTaskCreatePinnedToCore(temp_task, "temp_monitor", 3072, nullptr, 2, nullptr, 0) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "chip temperature sampling every %u ms", static_cast<unsigned>(TEMP_SAMPLE_MS));
    return ESP_OK;
}

bool temp_monitor_latest(float *out_celsius)
{
    if (!out_celsius || !s_mutex) {
        return false;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return false;
    }
    const bool ok = s_has_value;
    *out_celsius = s_latest;
    xSemaphoreGive(s_mutex);
    return ok;
}

size_t temp_monitor_history(float *out, size_t max)
{
    if (!out || max == 0 || !s_mutex) {
        return 0;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return 0;
    }
    const size_t count = s_count < max ? s_count : max;
    // Oldest sample starts at s_head once the ring is full, otherwise at 0.
    const size_t start = (s_count == TEMP_HISTORY_SIZE) ? s_head : 0;
    for (size_t i = 0; i < count; ++i) {
        out[i] = s_history[(start + s_count - count + i) % TEMP_HISTORY_SIZE];
    }
    xSemaphoreGive(s_mutex);
    return count;
}
