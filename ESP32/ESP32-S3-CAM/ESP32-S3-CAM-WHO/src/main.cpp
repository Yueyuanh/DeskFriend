#include "app_state.h"
#include "led_status.h"
#include "temp_monitor.h"
#include "vision_task.h"
#include "web_task.h"
#include "wifi_manager.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace {

constexpr char TAG[] = "deskfriend_app";
constexpr char SPIFFS_BASE[] = "/spiffs";

// NVS is required by Wi-Fi calibration data.
void init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

// Holds the enrolled face database (FACE_DB_PATH in vision_task.cpp).
void mount_storage()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE,
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

} // namespace

// Only boot order and task creation live here; every task body is in its own
// module (vision_task.cpp, web_task.cpp) and shares state through app_state.cpp.
extern "C" void app_main(void)
{
    init_nvs();
    mount_storage();

    ESP_ERROR_CHECK(app_state_init());
    // Start the status LED before Wi-Fi so the red blink is visible during the
    // whole association attempt; it turns green once an IP arrives.
    ESP_ERROR_CHECK(led_status_start());
    ESP_ERROR_CHECK(temp_monitor_start());
    ESP_ERROR_CHECK(wifi_manager_begin());
    ESP_ERROR_CHECK(visionTaskInstance.begin());

    BaseType_t vision_ok = xTaskCreatePinnedToCore(
        VisionTask_t::runTask, VisionTask_t::kTaskName, VisionTask_t::kStackSize, nullptr,
        VisionTask_t::kPriority, &visionTaskHandle, VisionTask_t::kCoreId);
    BaseType_t web_ok = xTaskCreatePinnedToCore(
        WebTask_t::runTask, WebTask_t::kTaskName, WebTask_t::kStackSize, nullptr,
        WebTask_t::kPriority, &webTaskHandle, WebTask_t::kCoreId);
    ESP_ERROR_CHECK((vision_ok == pdPASS && web_ok == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(TAG, "tasks started: vision(core %d, prio %d), web(core %d, prio %d)",
             static_cast<int>(VisionTask_t::kCoreId), static_cast<int>(VisionTask_t::kPriority),
             static_cast<int>(WebTask_t::kCoreId), static_cast<int>(WebTask_t::kPriority));
}
