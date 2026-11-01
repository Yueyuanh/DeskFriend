#include "vision/vision_task.h"

#include "core/app_state.h"
#include "vision/camera_manager.h"

#include "diag_config.h"

#include "dl_image_jpeg.hpp"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <list>

namespace {

constexpr char TAG[] = "deskfriend_vision";
constexpr char FACE_DB_PATH[] = "/spiffs/face.db";

#if DF_CAMERA_DIAG == 1
// Experiment A: is the sensor able to deliver pixels at all?
// Logs frame metadata, the first 16 bytes and per-channel statistics.
void diag_log_rgb565(int index, const camera_fb_t *fb)
{
    char hex[3 * 16 + 1] = {0};
    const size_t dump = fb->len < 16 ? fb->len : 16;
    for (size_t i = 0; i < dump; ++i) {
        std::snprintf(&hex[i * 3], 4, "%02X ", fb->buf[i]);
    }

    // If the DVP LSB line is stuck low, every byte is even.
    uint32_t odd_bytes = 0;
    for (size_t i = 0; i < fb->len; ++i) {
        if (fb->buf[i] & 0x01) {
            ++odd_bytes;
        }
    }

    const uint32_t pixels = static_cast<uint32_t>(fb->len / 2);
    uint8_t rmin = 255, rmax = 0, gmin = 255, gmax = 0, bmin = 255, bmax = 0;
    uint64_t rsum = 0, gsum = 0, bsum = 0;
    for (uint32_t i = 0; i < pixels; ++i) {
        const uint16_t v = static_cast<uint16_t>(fb->buf[2 * i] | (fb->buf[2 * i + 1] << 8));
        const uint8_t r = static_cast<uint8_t>(((v >> 11) & 0x1F) * 255 / 31);
        const uint8_t g = static_cast<uint8_t>(((v >> 5) & 0x3F) * 255 / 63);
        const uint8_t b = static_cast<uint8_t>((v & 0x1F) * 255 / 31);
        if (r < rmin) { rmin = r; }
        if (r > rmax) { rmax = r; }
        if (g < gmin) { gmin = g; }
        if (g > gmax) { gmax = g; }
        if (b < bmin) { bmin = b; }
        if (b > bmax) { bmax = b; }
        rsum += r; gsum += g; bsum += b;
    }

    ESP_LOGI(TAG, "DIAG A frame %d: %ux%u fmt=%d len=%u pixels=%u",
             index, static_cast<unsigned>(fb->width), static_cast<unsigned>(fb->height),
             static_cast<int>(fb->format), static_cast<unsigned>(fb->len), static_cast<unsigned>(pixels));
    ESP_LOGI(TAG, "DIAG A frame %d: first16=%s", index, hex);
    ESP_LOGI(TAG, "DIAG A frame %d: odd_bytes=%u/%u", index,
             static_cast<unsigned>(odd_bytes), static_cast<unsigned>(fb->len));
    if (pixels == 0) {
        return;
    }
    ESP_LOGI(TAG, "DIAG A frame %d: R[%u..%u avg %u] G[%u..%u avg %u] B[%u..%u avg %u]",
             index, rmin, rmax, static_cast<unsigned>(rsum / pixels),
             gmin, gmax, static_cast<unsigned>(gsum / pixels),
             bmin, bmax, static_cast<unsigned>(bsum / pixels));
    if (rmin == rmax && gmin == gmax && bmin == bmax) {
        ESP_LOGW(TAG, "DIAG A frame %d: constant frame (no pixel variation)", index);
    }
}

void diag_run_rgb565()
{
    ESP_LOGI(TAG, "DIAG A: RGB565 QQVGA, fb_count=1, GRAB_WHEN_EMPTY, PSRAM");
    for (int i = 0; i < 6; ++i) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "DIAG A frame %d: fb_get returned NULL (timeout)", i);
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }
        diag_log_rgb565(i, fb);
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    ESP_LOGI(TAG, "DIAG A: done");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

#if DF_CAMERA_DIAG == 2
// Experiment B: read back the registers the JPEG path is supposed to write.
// Read-only; capture behaviour is unchanged.
void diag_run_jpeg_regs()
{
    sensor_t *sensor = esp_camera_sensor_get();
    if (!sensor) {
        ESP_LOGE(TAG, "DIAG B: esp_camera_sensor_get returned NULL");
    } else {
        static const int regs[] = {
            0x300A, 0x300B, 0x4300, 0x3821, 0x4407, 0x501F,
            0x460C, 0x471C, 0x3002, 0x3006, 0x3035, 0x3036,
        };
        for (int reg : regs) {
            const int value = sensor->get_reg(sensor, reg, 0xFF) & 0xFF;
            ESP_LOGI(TAG, "DIAG B reg 0x%04X = 0x%02X", reg, value);
        }
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGW(TAG, "DIAG B: fb_get returned NULL (same NO-SOI path)");
    } else {
        ESP_LOGI(TAG, "DIAG B: fb ok len=%u", static_cast<unsigned>(fb->len));
        esp_camera_fb_return(fb);
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
#endif

// Face detection and recognition are throttled; between detections the cached
// boxes are redrawn on the running stream.
constexpr uint32_t FACE_DETECT_INTERVAL_MS = 250;
constexpr uint32_t RECOGNIZE_INTERVAL_MS = 1000;
// Recognition only runs once a second, so keep its result visible between runs
// instead of flipping the UI back to "未识别" on the very next frame.
constexpr uint32_t IDENTITY_HOLD_MS = 2500;
constexpr uint32_t FRAME_FAILURE_BACKOFF_MS = 30;
constexpr uint32_t FPS_WINDOW_US = 1000000;

// Converts a detector result into the frame-pixel box the web UI draws.
DetectionBox to_box(const dl::detect::result_t &result)
{
    DetectionBox box;
    if (result.box.size() < 4) {
        return box;
    }
    box.x = static_cast<int16_t>(std::max(0, result.box[0]));
    box.y = static_cast<int16_t>(std::max(0, result.box[1]));
    box.w = static_cast<int16_t>(std::max(0, result.box[2] - result.box[0]));
    box.h = static_cast<int16_t>(std::max(0, result.box[3] - result.box[1]));
    const float score = result.score * 100.0f;
    box.score = static_cast<uint8_t>(std::min(100.0f, std::max(0.0f, score)));
    return box;
}

} // namespace

VisionTask_t visionTaskInstance;
TaskHandle_t visionTaskHandle = nullptr;

esp_err_t VisionTask_t::begin()
{
    state_set_message("正在初始化摄像头");
    esp_err_t err = camera_manager_begin();
    if (err != ESP_OK) {
        state_set_message("摄像头初始化失败");
    }
    return err;
}

void VisionTask_t::run(void *pvParameters)
{
    (void)pvParameters;

#if DF_CAMERA_DIAG == 1
    diag_run_rgb565();
#elif DF_CAMERA_DIAG == 2
    diag_run_jpeg_regs();
#endif

    state_set_message("正在加载人脸模型");
    auto *face_detect = new HumanFaceDetect(HumanFaceDetect::MSRMNP_S8_V1, false);
    auto *recognizer = new HumanFaceRecognizer(FACE_DB_PATH, HumanFaceFeat::MFN_S8_V1, true);
    // The QVGA stream plus indoor lighting gives faces around 80-100 px, which
    // sits close to the default 0.5 score cut-off. Loosening both stages a
    // little improves recall without letting obvious false positives through.
    face_detect->set_score_thr(0.4f, 0);
    face_detect->set_score_thr(0.4f, 1);
    bool has_enrolled_face = recognizer->get_num_feats() > 0;
    state_set_face_db_count(recognizer->get_num_feats());
    state_set_message(has_enrolled_face ? "模型就绪，已加载人脸库" : "模型就绪，请注册人脸");

    uint32_t last_face_ms = 0;
    uint32_t last_recognition_ms = 0;
    uint32_t last_log_ms = 0;
    uint32_t last_identity_ms = 0;
    int installed_identity = -1;
    float installed_similarity = 0.0f;
    int last_face_count = 0;
    float last_fps = 0.0f;
    bool frame_size_known = false;
    DetectionBox held_face_boxes[MAX_DETECTION_BOXES];
    size_t held_face_count = 0;
    uint64_t fps_window_start = esp_timer_get_time();
    uint32_t fps_frames = 0;
    // Counts every published frame, including the ones sent while detection is
    // off, so the page can show the real MJPEG stream rate.
    uint64_t cam_window_start = esp_timer_get_time();
    uint32_t cam_frames = 0;

    for (;;) {
        const uint64_t frame_start = esp_timer_get_time();

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            state_set_message("摄像头取帧失败");
            vTaskDelay(pdMS_TO_TICKS(FRAME_FAILURE_BACKOFF_MS));
            continue;
        }

        // Publish first so the MJPEG stream keeps flowing even when inference
        // is paused from the web UI.
        state_publish_jpeg(fb->buf, fb->len);
        if (!frame_size_known) {
            state_set_frame_size(fb->width, fb->height);
            frame_size_known = true;
        }
        ++cam_frames;
        const uint64_t cam_elapsed = esp_timer_get_time() - cam_window_start;
        if (cam_elapsed >= FPS_WINDOW_US) {
            state_set_camera_fps(static_cast<float>(cam_frames) * 1000000.0f /
                                 static_cast<float>(cam_elapsed));
            cam_frames = 0;
            cam_window_start = esp_timer_get_time();
        }

        if (!state_face_enabled()) {
            esp_camera_fb_return(fb);
            state_set_boxes(nullptr, 0);
            // Zero the counters here too: the pause request races with the
            // detection results of the frame that was being processed.
            state_update_detections(0, -1, 0.0f, 0);
            state_set_inference_fps(0.0f);
            state_set_message("人脸检测已停止，画面仍在上传");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        dl::image::jpeg_img_t jpeg = {.data = fb->buf, .data_len = fb->len};
        auto img = dl::image::sw_decode_jpeg(jpeg, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
        esp_camera_fb_return(fb);
        if (!img.data) {
            state_set_message("JPEG 解码失败");
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        std::list<dl::detect::result_t> empty_faces;
        std::list<dl::detect::result_t> *faces = &empty_faces;
        bool ran_face = false;
        if (now_ms - last_face_ms >= FACE_DETECT_INTERVAL_MS) {
            faces = &face_detect->run(img);
            last_face_ms = now_ms;
            ran_face = true;
            // Keep the count stable between detection frames instead of
            // reporting zero three times out of four.
            last_face_count = static_cast<int>(faces->size());

            // Cache face boxes; they are redrawn on every frame while the
            // detector only runs four times a second.
            held_face_count = 0;
            for (const auto &face : *faces) {
                if (held_face_count >= MAX_DETECTION_BOXES) {
                    break;
                }
                held_face_boxes[held_face_count++] = to_box(face);
            }
        }

        state_set_boxes(held_face_boxes, held_face_count);

        if (state_take_clear_request()) {
            if (recognizer->clear_all_feats() == ESP_OK) {
                has_enrolled_face = false;
                installed_identity = -1;
                installed_similarity = 0.0f;
                state_set_face_db_count(0);
                state_set_message("人脸库已清空");
            } else {
                state_set_message("清空人脸库失败");
            }
        }

        if (ran_face && !faces->empty() && state_take_enroll_request()) {
            if (recognizer->enroll(img, *faces) == ESP_OK) {
                has_enrolled_face = true;
                state_set_face_db_count(recognizer->get_num_feats());
                state_finish_enroll_request();
                state_set_message("当前人脸已注册");
            } else {
                state_set_message("注册失败，请正对摄像头");
            }
        }

        int identity = -1;
        float similarity = 0.0f;
        if (has_enrolled_face && ran_face && !faces->empty() &&
            now_ms - last_recognition_ms >= RECOGNIZE_INTERVAL_MS) {
            auto matches = recognizer->recognize(img, *faces);
            if (!matches.empty()) {
                installed_identity = matches.front().id;
                installed_similarity = matches.front().similarity;
                last_identity_ms = now_ms;
            } else {
                installed_identity = -1;
                installed_similarity = 0.0f;
                last_identity_ms = now_ms;
                state_set_message("检测到人脸，但与已注册人脸不匹配");
            }
            last_recognition_ms = now_ms;
        }
        if (installed_identity >= 0 && now_ms - last_identity_ms <= IDENTITY_HOLD_MS) {
            identity = installed_identity;
            similarity = installed_similarity;
        }

        const uint32_t elapsed_ms = static_cast<uint32_t>((esp_timer_get_time() - frame_start) / 1000ULL);
        ++fps_frames;
        const uint64_t fps_elapsed = esp_timer_get_time() - fps_window_start;
        if (fps_elapsed >= FPS_WINDOW_US) {
            last_fps = static_cast<float>(fps_frames) * 1000000.0f / static_cast<float>(fps_elapsed);
            state_set_inference_fps(last_fps);
            fps_frames = 0;
            fps_window_start = esp_timer_get_time();
        }

        if (now_ms - last_log_ms >= 2000) {
            last_log_ms = now_ms;
            ESP_LOGI(TAG, "detect: faces=%d identity=%d sim=%.2f %lu ms %.1f fps",
                     last_face_count, identity, similarity,
                     static_cast<unsigned long>(elapsed_ms), last_fps);
        }

        state_update_detections(last_face_count, identity, similarity, elapsed_ms);

        heap_caps_free(img.data);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void VisionTask_t::runTask(void *pvParameters)
{
    visionTaskInstance.run(pvParameters);
}
