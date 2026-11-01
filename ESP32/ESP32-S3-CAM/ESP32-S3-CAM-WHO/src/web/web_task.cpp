#include "web/web_task.h"

#include "board/temp_monitor.h"
#include "core/app_state.h"
#include "core/tuning.h"
#include "index_html.h"

#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstdlib>

namespace {

constexpr char TAG[] = "deskfriend_web";
constexpr uint16_t SERVER_PORT = 80;
constexpr uint32_t HTTP_TASK_STACK = 8192;
constexpr size_t MAX_URI_HANDLERS = 10;

httpd_handle_t s_http_server = nullptr;

esp_err_t send_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t send_state(httpd_req_t *req)
{
    const VisionSnapshot snapshot = state_snapshot();
    const TuningParams tuning = tuning_get();
    float chip_celsius = 0.0f;
    const bool has_chip_temp = temp_monitor_latest(&chip_celsius);
    char chip_json[24];
    if (has_chip_temp) {
        std::snprintf(chip_json, sizeof(chip_json), "%.2f", chip_celsius);
    } else {
        std::snprintf(chip_json, sizeof(chip_json), "null");
    }

    static char json[1280];
    int used = std::snprintf(json, sizeof(json),
                             "{\"wifi\":%s,\"ip\":\"%s\",\"face_count\":%d,"
                             "\"identity\":\"%s\",\"inference_fps\":%.2f,\"inference_ms\":%lu,"
                             "\"frame_seq\":%lu,\"free_psram\":%u,\"message\":\"%s\","
                             "\"face_enabled\":%s,\"camera_fps\":%.2f,"
                             "\"face_db_count\":%d,"
                             "\"detect_thr\":%.2f,\"recognize_thr\":%.2f,"
                             "\"chip_temp\":%s,\"camera_temp\":null,"
                             "\"frame_w\":%u,\"frame_h\":%u,\"boxes\":[",
                             snapshot.wifi ? "true" : "false", snapshot.ip, snapshot.face_count,
                             snapshot.identity >= 0 ? "已识别" : "未识别",
                             snapshot.inference_fps,
                             static_cast<unsigned long>(snapshot.inference_ms),
                             static_cast<unsigned long>(snapshot.frame_seq),
                             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                             snapshot.message, snapshot.face_enabled ? "true" : "false",
                             snapshot.camera_fps, snapshot.face_db_count,
                             tuning.detect_score, tuning.recognize_sim, chip_json,
                             static_cast<unsigned>(snapshot.frame_width),
                             static_cast<unsigned>(snapshot.frame_height));
    if (used < 0) {
        used = 0;
    }
    for (uint8_t i = 0; i < snapshot.box_count && used < static_cast<int>(sizeof(json)) - 80; ++i) {
        const DetectionBox &box = snapshot.boxes[i];
        used += std::snprintf(json + used, sizeof(json) - used,
                              "%s{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"s\":%u}",
                              i ? "," : "", box.x, box.y, box.w, box.h, box.score);
    }
    std::snprintf(json + used, sizeof(json) - used, "]}");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

// Multi-minute chip temperature history for the chart on the page.
esp_err_t send_temps(httpd_req_t *req)
{
    static float values[TEMP_HISTORY_SIZE];
    const size_t count = temp_monitor_history(values, TEMP_HISTORY_SIZE);

    static char json[1024];
    int used = std::snprintf(json, sizeof(json),
                             "{\"interval_ms\":%u,\"chip\":[",
                             static_cast<unsigned>(TEMP_SAMPLE_MS));
    for (size_t i = 0; i < count && used < static_cast<int>(sizeof(json)) - 16; ++i) {
        used += std::snprintf(json + used, sizeof(json) - used, "%s%.2f", i ? "," : "",
                              values[i]);
    }
    std::snprintf(json + used, sizeof(json) - used, "],\"camera\":null}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

// GET /api/inference?face=0|1 switches face detection (and recognition) on or
// off; ?on=0|1 is an alias. The camera keeps publishing frames either way, so
// the MJPEG stream stays alive while inference is paused.
esp_err_t set_inference(httpd_req_t *req)
{
    char query[64] = {0};
    char value[8] = {0};
    bool face = state_face_enabled();
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "face", value, sizeof(value)) == ESP_OK) {
            face = value[0] != '0';
        } else if (httpd_query_key_value(query, "on", value, sizeof(value)) == ESP_OK) {
            face = value[0] != '0';
        }
    }
    state_set_face_enabled(face);
    state_set_message(face ? "人脸检测运行中" : "人脸检测已停止，画面仍在上传");

    char json[48];
    std::snprintf(json, sizeof(json), "{\"face\":%s}", face ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

// GET /api/tuning?detect=0.35&recognize=0.45 moves the sensitivity sliders.
// Omitted keys keep their current value; ?reset=1 restores the defaults.
// Values are clamped by tuning_set(), and the reply carries what was stored.
esp_err_t set_tuning(httpd_req_t *req)
{
    TuningParams params = tuning_get();
    bool reset = false;
    char query[96] = {0};
    char value[16] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "reset", value, sizeof(value)) == ESP_OK) {
            reset = value[0] != '0';
        }
        if (httpd_query_key_value(query, "detect", value, sizeof(value)) == ESP_OK) {
            params.detect_score = std::strtof(value, nullptr);
        }
        if (httpd_query_key_value(query, "recognize", value, sizeof(value)) == ESP_OK) {
            params.recognize_sim = std::strtof(value, nullptr);
        }
    }

    if (reset) {
        tuning_reset();
    } else {
        tuning_set(params);
    }

    const TuningParams applied = tuning_get();
    char json[80];
    std::snprintf(json, sizeof(json), "{\"detect\":%.2f,\"recognize\":%.2f}",
                  applied.detect_score, applied.recognize_sim);
    state_set_message(reset ? "灵敏度已恢复默认" : "灵敏度已更新");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

// One streaming client. Runs on its own task because esp_http_server executes
// handlers inline on a single server task; an endless MJPEG handler there would
// starve /api/state and the page itself.
void stream_task(void *arg)
{
    httpd_req_t *req = static_cast<httpd_req_t *>(arg);
    char header[96];
    uint32_t sent_seq = 0;

    for (;;) {
        // Send each captured frame once; the camera runs at ~5 fps, so pacing
        // on the frame counter avoids flooding the client with duplicates.
        const uint32_t seq = state_frame_seq();
        if (seq == sent_seq) {
            vTaskDelay(pdMS_TO_TICKS(15));
            continue;
        }

        uint8_t *copy = nullptr;
        size_t len = 0;
        if (!state_copy_jpeg(&copy, &len)) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        sent_seq = seq;

        const int header_len = std::snprintf(header, sizeof(header),
                                             "--frame\r\nContent-Type: image/jpeg\r\n"
                                             "Content-Length: %u\r\n\r\n",
                                             static_cast<unsigned>(len));
        esp_err_t err = httpd_resp_send_chunk(req, header, header_len);
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(copy), len);
        }
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, "\r\n", 2);
        }
        heap_caps_free(copy);

        // A failed chunk send means the browser closed the stream.
        if (err != ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    httpd_req_async_handler_complete(req);
    vTaskDelete(nullptr);
}

// multipart/x-mixed-replace stream consumed by <img src="/stream">.
esp_err_t send_stream(httpd_req_t *req)
{
    httpd_req_t *async_req = nullptr;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        return ESP_FAIL;
    }
    httpd_resp_set_type(async_req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(async_req, "Cache-Control", "no-store");

    if (xTaskCreate(stream_task, "mjpeg_stream", 6144, async_req, 4, nullptr) != pdPASS) {
        httpd_req_async_handler_complete(async_req);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t send_capture(httpd_req_t *req)
{
    uint8_t *copy = nullptr;
    size_t len = 0;
    if (!state_copy_jpeg(&copy, &len)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "frame not ready");
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, reinterpret_cast<const char *>(copy), len);
    heap_caps_free(copy);
    return err;
}

esp_err_t request_enroll(httpd_req_t *req)
{
    state_request_enroll();
    state_set_message("等待下一张人脸注册");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"message\":\"等待下一张人脸注册\"}");
}

esp_err_t request_clear(httpd_req_t *req)
{
    state_request_clear();
    state_set_message("正在清空人脸库");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"message\":\"正在清空人脸库\"}");
}

} // namespace

WebTask_t webTaskInstance;
TaskHandle_t webTaskHandle = nullptr;

void WebTask_t::run(void *pvParameters)
{
    (void)pvParameters;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = SERVER_PORT;
    config.stack_size = HTTP_TASK_STACK;
    config.max_uri_handlers = MAX_URI_HANDLERS;
    config.lru_purge_enable = true;
    ESP_ERROR_CHECK(httpd_start(&s_http_server, &config));

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = send_index, .user_ctx = nullptr};
    httpd_uri_t state = {.uri = "/api/state", .method = HTTP_GET, .handler = send_state, .user_ctx = nullptr};
    httpd_uri_t capture = {.uri = "/capture", .method = HTTP_GET, .handler = send_capture, .user_ctx = nullptr};
    httpd_uri_t stream = {.uri = "/stream", .method = HTTP_GET, .handler = send_stream, .user_ctx = nullptr};
    httpd_uri_t temps = {.uri = "/api/temps", .method = HTTP_GET, .handler = send_temps, .user_ctx = nullptr};
    httpd_uri_t inference = {.uri = "/api/inference", .method = HTTP_GET, .handler = set_inference, .user_ctx = nullptr};
    httpd_uri_t tuning = {.uri = "/api/tuning", .method = HTTP_GET, .handler = set_tuning, .user_ctx = nullptr};
    httpd_uri_t enroll = {.uri = "/api/enroll", .method = HTTP_POST, .handler = request_enroll, .user_ctx = nullptr};
    httpd_uri_t clear = {.uri = "/api/clear", .method = HTTP_POST, .handler = request_clear, .user_ctx = nullptr};
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &state));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &capture));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &stream));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &temps));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &inference));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &tuning));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &enroll));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &clear));

    ESP_LOGI(TAG, "web server started on port %d", config.server_port);
    // esp_http_server owns its own worker task from here on.
    vTaskDelete(nullptr);
}

void WebTask_t::runTask(void *pvParameters)
{
    webTaskInstance.run(pvParameters);
}
