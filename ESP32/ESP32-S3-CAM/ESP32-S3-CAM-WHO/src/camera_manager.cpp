#include "camera_manager.h"

#include "app_config.h"
#include "diag_config.h"

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace {

constexpr char TAG[] = "deskfriend_camera";

// Capture parameters for QVGA JPEG on the Goouuu ESP32-S3-CAM + OV5640.
// Pins come from include/app_config.h; keep every other camera knob here.
constexpr int XCLK_FREQ_HZ = 20000000;
constexpr int JPEG_QUALITY = 12;
#if DF_CAMERA_DIAG == 1
constexpr int FRAME_BUFFER_COUNT = 1;
#else
constexpr int FRAME_BUFFER_COUNT = 3;
#endif

camera_config_t make_config()
{
    camera_config_t config = {};
    config.pin_pwdn = DF_CAM_PWDN;
    config.pin_reset = DF_CAM_RESET;
    config.pin_xclk = DF_CAM_XCLK;
    config.pin_sccb_sda = DF_CAM_SIOD;
    config.pin_sccb_scl = DF_CAM_SIOC;
    config.pin_d7 = DF_CAM_D7;
    config.pin_d6 = DF_CAM_D6;
    config.pin_d5 = DF_CAM_D5;
    config.pin_d4 = DF_CAM_D4;
    config.pin_d3 = DF_CAM_D3;
    config.pin_d2 = DF_CAM_D2;
    config.pin_d1 = DF_CAM_D1;
    config.pin_d0 = DF_CAM_D0;
    config.pin_vsync = DF_CAM_VSYNC;
    config.pin_href = DF_CAM_HREF;
    config.pin_pclk = DF_CAM_PCLK;
    config.xclk_freq_hz = XCLK_FREQ_HZ;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
#if DF_CAMERA_DIAG == 1
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QQVGA;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
#else
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.grab_mode = CAMERA_GRAB_LATEST;
#endif
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = FRAME_BUFFER_COUNT;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    return config;
}

bool s_initialized = false;

} // namespace

esp_err_t camera_manager_begin()
{
    if (s_initialized) {
        return ESP_OK;
    }

    camera_config_t config = make_config();
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s (0x%x)", esp_err_to_name(err), err);
        return err;
    }
    s_initialized = true;

    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        ESP_LOGW(TAG, "esp_camera_sensor_get returned NULL");
    } else {
        sensor->set_vflip(sensor, 1);
        ESP_LOGI(TAG, "sensor PID 0x%04x, format JPEG, QVGA, quality %d, fb_count %d",
                 static_cast<unsigned>(sensor->id.PID), JPEG_QUALITY, FRAME_BUFFER_COUNT);
    }
    ESP_LOGI(TAG, "camera ready, PSRAM free=%u bytes",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    return ESP_OK;
}

esp_err_t camera_manager_end()
{
    if (!s_initialized) {
        return ESP_OK;
    }
    esp_err_t err = esp_camera_deinit();
    if (err == ESP_OK) {
        s_initialized = false;
    }
    return err;
}
