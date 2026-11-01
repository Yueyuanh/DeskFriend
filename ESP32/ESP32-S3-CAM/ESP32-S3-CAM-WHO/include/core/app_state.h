#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

// One face box in frame pixel coordinates (frame_width x frame_height).
struct DetectionBox {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    uint8_t score = 0;  // 0..100
};

constexpr size_t MAX_DETECTION_BOXES = 8;

// Snapshot of everything the HTTP layer reports about the vision pipeline.
// Plain data (no pointers) so it can be copied out under the state mutex.
struct VisionSnapshot {
    bool wifi = false;
    char ip[16] = "0.0.0.0";
    int face_count = 0;
    int identity = -1;
    float similarity = 0.0f;
    float inference_fps = 0.0f;
    uint32_t inference_ms = 0;
    uint32_t frame_seq = 0;
    bool face_enabled = true;
    float camera_fps = 0.0f;
    int face_db_count = 0;
    uint16_t frame_width = 0;
    uint16_t frame_height = 0;
    DetectionBox boxes[MAX_DETECTION_BOXES];
    uint8_t box_count = 0;
    char message[96] = "正在初始化模型";
};

// Creates the mutexes used by every other app_state_* function.
// Must be called once from app_main() before the tasks are created.
esp_err_t app_state_init();

// Wi-Fi status, written by the Wi-Fi event handler.
void state_set_wifi(bool connected, const char *ip);
// Cheap read for the status LED task (no snapshot copy).
bool state_wifi_connected();

// Human readable pipeline status shown by the web UI.
void state_set_message(const char *message);

// Keeps the newest JPEG frame in PSRAM for the /capture endpoint.
void state_publish_jpeg(const uint8_t *data, size_t len);
// Copies the newest JPEG into a new PSRAM buffer. The caller frees it with
// heap_caps_free(). Returns false when no frame is available.
bool state_copy_jpeg(uint8_t **out, size_t *len);

VisionSnapshot state_snapshot();

void state_update_detections(int face_count, int identity, float similarity,
                             uint32_t inference_ms);
void state_set_inference_fps(float fps);

// Face detection switch. When off the vision task still publishes frames, so
// the MJPEG stream keeps going while inference is skipped.
void state_set_face_enabled(bool enabled);
bool state_face_enabled();

// Frame rate of the MJPEG pipeline (frames published per second).
void state_set_camera_fps(float fps);

// Number of face feature vectors currently stored in the face database.
void state_set_face_db_count(int count);

// Monotonic counter incremented for every published frame. Lets the MJPEG
// stream avoid re-sending the same JPEG.
uint32_t state_frame_seq();

// Detection boxes for the newest analysed frame, drawn by the web UI.
void state_set_boxes(const DetectionBox *boxes, size_t count);
// Frame geometry of the newest published JPEG. Updated on every frame, so it
// tracks sensor reconfiguration driven by the UVC host.
void state_set_frame_size(uint16_t width, uint16_t height);

// Camera frame size (framesize_t value, kept as int so callers do not need
// esp_camera.h). The UVC host posts a request here; the vision task owns the
// sensor and consumes it between frames. The newest request wins.
void state_request_frame_size(int frame_size);
bool state_take_frame_size_request(int *frame_size);

// Enrollment / clear commands: the web task posts, the vision task consumes.
void state_request_enroll();
void state_request_clear();
bool state_take_enroll_request();  // peek; cleared by state_finish_enroll_request()
bool state_take_clear_request();   // read and clear
void state_finish_enroll_request();
