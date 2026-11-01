#include "app_state.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr char TAG[] = "deskfriend_state";
constexpr uint32_t LOCK_TIMEOUT_MS = 100;

// Shared between the vision task (writer) and the web task (reader).
struct VisionState {
    bool wifi = false;
    char ip[16] = "0.0.0.0";
    int face_count = 0;
    int identity = -1;
    float similarity = 0.0f;
    float inference_fps = 0.0f;
    uint32_t inference_ms = 0;
    uint32_t frame_seq = 0;
    char message[96] = "正在初始化模型";
    bool face_enabled = true;
    float camera_fps = 0.0f;
    int face_db_count = 0;
    uint16_t frame_width = 0;
    uint16_t frame_height = 0;
    DetectionBox boxes[MAX_DETECTION_BOXES];
    uint8_t box_count = 0;
    uint8_t *jpeg = nullptr;
    size_t jpeg_len = 0;
};

VisionState s_state;
SemaphoreHandle_t s_state_mutex = nullptr;
SemaphoreHandle_t s_request_mutex = nullptr;
bool s_enroll_request = false;
bool s_clear_request = false;

bool lock(SemaphoreHandle_t mutex)
{
    return xSemaphoreTake(mutex, pdMS_TO_TICKS(LOCK_TIMEOUT_MS)) == pdTRUE;
}

} // namespace

esp_err_t app_state_init()
{
    if (s_state_mutex && s_request_mutex) {
        return ESP_OK;
    }
    s_state_mutex = xSemaphoreCreateMutex();
    s_request_mutex = xSemaphoreCreateMutex();
    return (s_state_mutex && s_request_mutex) ? ESP_OK : ESP_ERR_NO_MEM;
}

void state_set_wifi(bool connected, const char *ip)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.wifi = connected;
    if (ip) {
        std::snprintf(s_state.ip, sizeof(s_state.ip), "%s", ip);
    }
    xSemaphoreGive(s_state_mutex);
}

// Cheap read used by the status LED task, which polls every 20 ms.
bool state_wifi_connected()
{
    if (!lock(s_state_mutex)) {
        return false;
    }
    const bool connected = s_state.wifi;
    xSemaphoreGive(s_state_mutex);
    return connected;
}

void state_set_message(const char *message)
{
    if (!message || !lock(s_state_mutex)) {
        return;
    }
    std::snprintf(s_state.message, sizeof(s_state.message), "%s", message);
    xSemaphoreGive(s_state_mutex);
}

void state_publish_jpeg(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }
    auto *copy = static_cast<uint8_t *>(heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!copy) {
        ESP_LOGW(TAG, "JPEG copy allocation failed (%u bytes)", static_cast<unsigned>(len));
        return;
    }
    std::memcpy(copy, data, len);

    uint8_t *old = nullptr;
    if (lock(s_state_mutex)) {
        old = s_state.jpeg;
        s_state.jpeg = copy;
        s_state.jpeg_len = len;
        ++s_state.frame_seq;  // advances even while inference is paused
        xSemaphoreGive(s_state_mutex);
    } else {
        heap_caps_free(copy);
    }
    if (old) {
        heap_caps_free(old);
    }
}

bool state_copy_jpeg(uint8_t **out, size_t *len)
{
    if (!out || !len) {
        return false;
    }
    *out = nullptr;
    *len = 0;
    if (!lock(s_state_mutex)) {
        return false;
    }
    const size_t size = s_state.jpeg_len;
    if (size > 0 && s_state.jpeg) {
        auto *copy = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (copy) {
            std::memcpy(copy, s_state.jpeg, size);
            *out = copy;
            *len = size;
        }
    }
    xSemaphoreGive(s_state_mutex);
    return *out != nullptr;
}

VisionSnapshot state_snapshot()
{
    VisionSnapshot snapshot{};
    if (!lock(s_state_mutex)) {
        return snapshot;
    }
    snapshot.wifi = s_state.wifi;
    std::snprintf(snapshot.ip, sizeof(snapshot.ip), "%s", s_state.ip);
    snapshot.face_count = s_state.face_count;
    snapshot.identity = s_state.identity;
    snapshot.similarity = s_state.similarity;
    snapshot.inference_fps = s_state.inference_fps;
    snapshot.inference_ms = s_state.inference_ms;
    snapshot.frame_seq = s_state.frame_seq;
    snapshot.face_enabled = s_state.face_enabled;
    snapshot.camera_fps = s_state.camera_fps;
    snapshot.face_db_count = s_state.face_db_count;
    snapshot.frame_width = s_state.frame_width;
    snapshot.frame_height = s_state.frame_height;
    snapshot.box_count = s_state.box_count;
    for (uint8_t i = 0; i < s_state.box_count; ++i) {
        snapshot.boxes[i] = s_state.boxes[i];
    }
    std::snprintf(snapshot.message, sizeof(snapshot.message), "%s", s_state.message);
    xSemaphoreGive(s_state_mutex);
    return snapshot;
}

void state_update_detections(int face_count, int identity, float similarity,
                             uint32_t inference_ms)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.face_count = face_count;
    s_state.identity = identity;
    s_state.similarity = similarity;
    s_state.inference_ms = inference_ms;
    xSemaphoreGive(s_state_mutex);
}

void state_set_inference_fps(float fps)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.inference_fps = fps;
    xSemaphoreGive(s_state_mutex);
}

void state_set_face_enabled(bool enabled)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.face_enabled = enabled;
    if (!enabled) {
        s_state.face_count = 0;
        s_state.identity = -1;
        s_state.similarity = 0.0f;
    }
    xSemaphoreGive(s_state_mutex);
}

bool state_face_enabled()
{
    if (!lock(s_state_mutex)) {
        return false;
    }
    const bool enabled = s_state.face_enabled;
    xSemaphoreGive(s_state_mutex);
    return enabled;
}

void state_set_camera_fps(float fps)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.camera_fps = fps;
    xSemaphoreGive(s_state_mutex);
}

void state_set_face_db_count(int count)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.face_db_count = count;
    xSemaphoreGive(s_state_mutex);
}

uint32_t state_frame_seq()
{
    if (!lock(s_state_mutex)) {
        return 0;
    }
    const uint32_t seq = s_state.frame_seq;
    xSemaphoreGive(s_state_mutex);
    return seq;
}

void state_set_boxes(const DetectionBox *boxes, size_t count)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    if (count > MAX_DETECTION_BOXES) {
        count = MAX_DETECTION_BOXES;
    }
    s_state.box_count = static_cast<uint8_t>(count);
    for (size_t i = 0; i < count; ++i) {
        s_state.boxes[i] = boxes[i];
    }
    xSemaphoreGive(s_state_mutex);
}

void state_set_frame_size(uint16_t width, uint16_t height)
{
    if (!lock(s_state_mutex)) {
        return;
    }
    s_state.frame_width = width;
    s_state.frame_height = height;
    xSemaphoreGive(s_state_mutex);
}

void state_request_enroll()
{
    if (!lock(s_request_mutex)) {
        return;
    }
    s_enroll_request = true;
    xSemaphoreGive(s_request_mutex);
}

void state_request_clear()
{
    if (!lock(s_request_mutex)) {
        return;
    }
    s_clear_request = true;
    xSemaphoreGive(s_request_mutex);
}

bool state_take_enroll_request()
{
    bool requested = false;
    if (lock(s_request_mutex)) {
        requested = s_enroll_request;
        xSemaphoreGive(s_request_mutex);
    }
    return requested;
}

bool state_take_clear_request()
{
    bool requested = false;
    if (lock(s_request_mutex)) {
        requested = s_clear_request;
        s_clear_request = false;
        xSemaphoreGive(s_request_mutex);
    }
    return requested;
}

void state_finish_enroll_request()
{
    if (!lock(s_request_mutex)) {
        return;
    }
    s_enroll_request = false;
    xSemaphoreGive(s_request_mutex);
}
