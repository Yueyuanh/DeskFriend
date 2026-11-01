#include "usb/uvc_camera.h"

#include <sys/time.h>

#include "core/app_state.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb_device_uvc.h"

namespace {

constexpr char TAG[] = "deskfriend_uvc";

// Must hold one complete JPEG. The UVC component copies each frame here before
// handing it to TinyUSB. 128 KB covers the largest advertised mode: the camera
// driver caps an SVGA JPEG at 800*600/5 = 96 KB (CONFIG_CAMERA_JPEG_MODE_FRAME_SIZE_AUTO).
constexpr size_t kUvcBufferSize = 128 * 1024;

// Modes advertised by the UVC descriptors (sdkconfig.defaults). The host picks
// one; the vision task reinitializes the sensor to match, and the whole
// pipeline (Wi-Fi stream, detection, recognition) follows that geometry.
struct FrameMode {
    int width;
    int height;
    framesize_t frame_size;
};
constexpr FrameMode kFrameModes[] = {
    {320, 240, FRAMESIZE_QVGA},
    {640, 480, FRAMESIZE_VGA},
    {480, 320, FRAMESIZE_HVGA},
    {800, 600, FRAMESIZE_SVGA},
};

uvc_fb_t s_fb;
uint8_t *s_uvc_buffer = nullptr;
volatile bool s_streaming = false;
volatile int s_target_width = 0;
volatile int s_target_height = 0;
bool s_size_mismatch_logged = false;

const FrameMode *find_mode(int width, int height)
{
    for (const FrameMode &mode : kFrameModes) {
        if (mode.width == width && mode.height == height) {
            return &mode;
        }
    }
    return nullptr;
}

// The vision task publishes a frame every ~90 ms at QVGA, less often at larger
// modes. A host can request a frame before that has happened, so wait briefly
// instead of failing and printing an error every interval.
bool copy_latest_jpeg(uint8_t **buf, size_t *len)
{
    // At SVGA one vision loop (decode + detection) can take close to a second,
    // so allow a generous wait before reporting "no frame" to the UVC task.
    for (int attempt = 0; attempt < 400; ++attempt) {
        if (state_copy_jpeg(buf, len) && *len > 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return false;
}

// Blocks the UVC video task (not the TinyUSB control task) until the sensor has
// been reconfigured and a frame at the requested geometry has been published.
bool wait_for_frame_size(uint16_t width, uint16_t height, uint32_t timeout_ms)
{
    const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(timeout_ms) * 1000;
    while (esp_timer_get_time() < deadline) {
        const VisionSnapshot snapshot = state_snapshot();
        if (snapshot.frame_width == width && snapshot.frame_height == height) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

esp_err_t uvc_start_cb(uvc_format_t format, int width, int height, int rate, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "host opened stream: format=%d %dx%d @%d fps", static_cast<int>(format),
             width, height, rate);
    if (format != UVC_FORMAT_JPEG) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const FrameMode *mode = find_mode(width, height);
    if (!mode) {
        ESP_LOGW(TAG, "unsupported frame size %dx%d", width, height);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Return immediately: the streaming task waits for the matching geometry in
    // fb_get_cb, so this control transfer does not stall while the sensor is
    // reinitialized.
    s_target_width = width;
    s_target_height = height;
    s_size_mismatch_logged = false;
    state_request_frame_size(static_cast<int>(mode->frame_size));
    s_streaming = true;
    return ESP_OK;
}

uvc_fb_t *uvc_fb_get_cb(void *ctx)
{
    (void)ctx;

    const int target_width = s_target_width;
    const int target_height = s_target_height;
    if (target_width > 0 && target_height > 0 &&
        !wait_for_frame_size(static_cast<uint16_t>(target_width),
                             static_cast<uint16_t>(target_height), 5000)) {
        if (!s_size_mismatch_logged) {
            s_size_mismatch_logged = true;
            ESP_LOGW(TAG, "timed out waiting for %dx%d; sending whatever the camera produces",
                     target_width, target_height);
        }
    }

    if (!copy_latest_jpeg(&s_fb.buf, &s_fb.len)) {
        return nullptr;
    }
    gettimeofday(&s_fb.timestamp, nullptr);

    const VisionSnapshot snapshot = state_snapshot();
    s_fb.width = snapshot.frame_width ? snapshot.frame_width : kFrameModes[0].width;
    s_fb.height = snapshot.frame_height ? snapshot.frame_height : kFrameModes[0].height;
    s_fb.format = UVC_FORMAT_JPEG;
    return &s_fb;
}

void uvc_fb_return_cb(uvc_fb_t *fb, void *ctx)
{
    (void)ctx;
    if (fb && fb->buf) {
        // state_copy_jpeg() hands over a PSRAM copy owned by the caller.
        heap_caps_free(fb->buf);
        fb->buf = nullptr;
        fb->len = 0;
    }
}

void uvc_stop_cb(void *ctx)
{
    (void)ctx;
    s_streaming = false;
    s_target_width = 0;
    s_target_height = 0;
    // Back to the fast mode once no host is watching the wired feed, so Wi-Fi
    // streaming and inference recover their frame rate after the app closes.
    state_request_frame_size(FRAMESIZE_QVGA);
    ESP_LOGI(TAG, "host closed stream");
}

} // namespace

bool uvc_camera_streaming()
{
    return s_streaming;
}

esp_err_t uvc_camera_start()
{
    // The component memcpy()s each frame into this buffer before handing it to
    // TinyUSB, so it does not need to be DMA-capable. Internal RAM is still
    // preferred (smaller PSRAM cache impact); 128 KB after Wi-Fi and the models
    // is not always available there, hence the PSRAM fallback.
    s_uvc_buffer = static_cast<uint8_t *>(
        heap_caps_malloc(kUvcBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!s_uvc_buffer) {
        ESP_LOGW(TAG, "internal RAM for UVC buffer unavailable, trying PSRAM");
        s_uvc_buffer = static_cast<uint8_t *>(
            heap_caps_malloc(kUvcBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (!s_uvc_buffer) {
        ESP_LOGE(TAG, "UVC buffer allocation failed (%u bytes)",
                 static_cast<unsigned>(kUvcBufferSize));
        return ESP_ERR_NO_MEM;
    }

    uvc_device_config_t config = {};
    config.uvc_buffer = s_uvc_buffer;
    config.uvc_buffer_size = kUvcBufferSize;
    config.start_cb = uvc_start_cb;
    config.fb_get_cb = uvc_fb_get_cb;
    config.fb_return_cb = uvc_fb_return_cb;
    config.stop_cb = uvc_stop_cb;

    esp_err_t err = uvc_device_config(0, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uvc_device_config failed: %s", esp_err_to_name(err));
        return err;
    }
    err = uvc_device_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uvc_device_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "USB UVC webcam ready: QVGA/HVGA/VGA/SVGA MJPEG on the OTG Type-C port");
    return ESP_OK;
}
