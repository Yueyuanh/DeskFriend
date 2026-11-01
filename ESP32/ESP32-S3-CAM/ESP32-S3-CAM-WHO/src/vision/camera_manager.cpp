#include "vision/camera_manager.h"

#include "app_config.h"
#include "diag_config.h"

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace {

constexpr char TAG[] = "deskfriend_camera";

// Capture parameters for JPEG on the Goouuu ESP32-S3-CAM + OV5640.
// Pins come from include/app_config.h; keep every other camera knob here.
constexpr int XCLK_FREQ_HZ = 20000000;
constexpr int JPEG_QUALITY = 12;
#if DF_CAMERA_DIAG == 1
constexpr int FRAME_BUFFER_COUNT = 1;
#else
constexpr int FRAME_BUFFER_COUNT = 3;
#endif

const char *framesize_name(framesize_t frame_size)
{
    switch (frame_size) {
    case FRAMESIZE_QVGA:
        return "QVGA 320x240";
    case FRAMESIZE_HVGA:
        return "HVGA 480x320";
    case FRAMESIZE_VGA:
        return "VGA 640x480";
    case FRAMESIZE_SVGA:
        return "SVGA 800x600";
    default:
        return "custom";
    }
}

camera_config_t make_config(framesize_t frame_size)
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
    config.frame_size = frame_size;
    config.grab_mode = CAMERA_GRAB_LATEST;
#endif
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = FRAME_BUFFER_COUNT;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    return config;
}

bool s_initialized = false;
// QVGA is the fast default for Wi-Fi streaming and inference. A UVC host can
// move the sensor to a larger mode; the pipeline follows it.
framesize_t s_frame_size = FRAMESIZE_QVGA;

} // namespace

esp_err_t camera_manager_begin()
{
    if (s_initialized) {
        return ESP_OK;
    }

    camera_config_t config = make_config(s_frame_size);
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
        ESP_LOGI(TAG, "sensor PID 0x%04x, format JPEG, %s, quality %d, fb_count %d",
                 static_cast<unsigned>(sensor->id.PID), framesize_name(s_frame_size),
                 JPEG_QUALITY, FRAME_BUFFER_COUNT);
    }
    ESP_LOGI(TAG, "camera ready, PSRAM free=%u bytes",
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    return ESP_OK;
}

esp_err_t camera_manager_set_framesize(framesize_t frame_size)
{
    if (s_initialized && frame_size == s_frame_size) {
        return ESP_OK;
    }

    if (s_initialized) {
        esp_err_t err = camera_manager_end();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_camera_deinit failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    s_frame_size = frame_size;
    esp_err_t err = camera_manager_begin();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "camera reconfigured to %s", framesize_name(frame_size));
        return ESP_OK;
    }

    // Never leave the board without a camera: restore the known-good mode.
    ESP_LOGW(TAG, "%s unavailable, falling back to QVGA", framesize_name(frame_size));
    s_frame_size = FRAMESIZE_QVGA;
    const esp_err_t fallback_err = camera_manager_begin();
    // Reported as an error so the UI says the switch failed even though QVGA
    // brought the stream back.
    return fallback_err == ESP_OK ? ESP_ERR_NOT_SUPPORTED : fallback_err;
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
