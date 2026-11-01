#include "usb/usb_link.h"

#include "board/led_status.h"
#include "board/temp_monitor.h"
#include "core/app_state.h"
#include "core/tuning.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

constexpr char TAG[] = "deskfriend_link";
constexpr uint32_t TELEMETRY_INTERVAL_MS = 500;
constexpr uint32_t LOOP_INTERVAL_MS = 50;
constexpr size_t CMD_LINE_MAX = 128;
constexpr size_t TELEMETRY_MAX = 512;

char s_line[CMD_LINE_MAX];
size_t s_line_len = 0;
bool s_connected = false;

// Writes until the whole string is queued. The CDC FIFO is 512 bytes, so a
// telemetry line fits, but a burst of ACKs can need a retry.
void send_text(const char *text)
{
    const size_t len = std::strlen(text);
    size_t sent = 0;
    while (sent < len && tud_cdc_connected()) {
        const size_t wrote = tud_cdc_write(text + sent, len - sent);
        tud_cdc_write_flush();
        if (wrote == 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        sent += wrote;
    }
}

void send_telemetry()
{
    const VisionSnapshot snapshot = state_snapshot();
    float celsius = 0.0f;
    char temperature[16];
    if (temp_monitor_latest(&celsius)) {
        std::snprintf(temperature, sizeof(temperature), "%.1f", celsius);
    } else {
        std::snprintf(temperature, sizeof(temperature), "null");
    }

    char line[TELEMETRY_MAX];
    int used = std::snprintf(line, sizeof(line),
                             "{\"t\":%s,\"fps\":%.1f,\"w\":%u,\"h\":%u,"
                             "\"id\":%d,\"sim\":%.2f,\"db\":%d,\"faces\":[",
                             temperature, snapshot.camera_fps,
                             static_cast<unsigned>(snapshot.frame_width),
                             static_cast<unsigned>(snapshot.frame_height),
                             snapshot.identity, snapshot.similarity,
                             snapshot.face_db_count);
    if (used < 0) {
        return;
    }
    for (uint8_t i = 0; i < snapshot.box_count && used < static_cast<int>(sizeof(line)) - 64; ++i) {
        const DetectionBox &box = snapshot.boxes[i];
        used += std::snprintf(line + used, sizeof(line) - used,
                              "%s[%d,%d,%d,%d,%u]", i ? "," : "", box.x, box.y, box.w, box.h,
                              box.score);
    }
    std::snprintf(line + used, sizeof(line) - used, "]}\n");
    send_text(line);
}

// Host commands are intentionally free-form so the PC side can prototype
// servo angles or anything else without a firmware change. A few get a
// meaning; everything is still logged and acknowledged.
void handle_command(const char *line)
{
    ESP_LOGI(TAG, "RX: %s", line);

    if (std::strncmp(line, "FACE ", 5) == 0) {
        const bool enabled = line[5] != '0';
        state_set_face_enabled(enabled);
        state_set_message(enabled ? "人脸检测运行中（USB 控制）" : "人脸检测已停止（USB 控制）");
    } else if (std::strncmp(line, "THR ", 4) == 0) {
        // "THR <detect> <recognize>", same ranges as the web sliders.
        float detect = 0.0f;
        float recognize = 0.0f;
        if (std::sscanf(line + 4, "%f %f", &detect, &recognize) == 2) {
            TuningParams params = tuning_get();
            if (detect > 0.0f) {
                params.detect_score = detect;
            }
            if (recognize > 0.0f) {
                params.recognize_sim = recognize;
            }
            tuning_set(params);
            state_set_message("灵敏度已更新（USB 控制）");
        }
    } else if (std::strcmp(line, "ENROLL") == 0) {
        state_request_enroll();
        state_set_message("等待下一张人脸注册（USB 控制）");
    } else if (std::strcmp(line, "CLEAR") == 0) {
        state_request_clear();
        state_set_message("正在清空人脸库（USB 控制）");
    } else if (std::strncmp(line, "LED ", 4) == 0) {
        // "LED <r> <g> <b> [ms]" drives the status pixel for a few seconds so
        // the host can check its colour order without reflashing.
        int r = 0, g = 0, b = 0;
        int hold_ms = 0;
        const int matched = std::sscanf(line + 4, "%d %d %d %d", &r, &g, &b, &hold_ms);
        if (matched >= 3) {
            led_status_flash(static_cast<uint8_t>(std::clamp(r, 0, 255)),
                             static_cast<uint8_t>(std::clamp(g, 0, 255)),
                             static_cast<uint8_t>(std::clamp(b, 0, 255)),
                             hold_ms > 0 ? static_cast<uint32_t>(hold_ms) : 0);
            state_set_message("状态灯测试（USB 控制）");
        }
    } else if (std::strcmp(line, "PING") == 0) {
        send_text("PONG\n");
        return;
    }

    char ack[CMD_LINE_MAX + 8];
    std::snprintf(ack, sizeof(ack), "ACK %s\n", line);
    send_text(ack);
}

void link_task(void *arg)
{
    (void)arg;
    uint32_t last_telemetry_ms = 0;
    uint8_t rx[64];

    for (;;) {
        const bool connected = tud_cdc_connected();
        if (connected && !s_connected) {
            ESP_LOGI(TAG, "CDC link up");
        } else if (!connected && s_connected) {
            ESP_LOGI(TAG, "CDC link down");
        }
        s_connected = connected;

        if (!connected) {
            s_line_len = 0;
            vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL_MS));
            continue;
        }

        const uint32_t available = tud_cdc_available();
        if (available > 0) {
            const size_t count = tud_cdc_read(rx, std::min<size_t>(available, sizeof(rx)));
            for (size_t i = 0; i < count; ++i) {
                const char c = static_cast<char>(rx[i]);
                if (c == '\r') {
                    continue;
                }
                if (c == '\n') {
                    s_line[s_line_len] = '\0';
                    if (s_line_len > 0) {
                        handle_command(s_line);
                    }
                    s_line_len = 0;
                    continue;
                }
                if (s_line_len + 1 < CMD_LINE_MAX) {
                    s_line[s_line_len++] = c;
                }
            }
        }

        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        if (now_ms - last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
            last_telemetry_ms = now_ms;
            send_telemetry();
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL_MS));
    }
}

} // namespace

esp_err_t usb_link_start()
{
    // Same core as the TinyUSB task: the link only makes short calls into
    // TinyUSB and waits most of the time.
    if (xTaskCreatePinnedToCore(link_task, "usb_link", 4096, nullptr, 3, nullptr, 0) != pdPASS) {
        ESP_LOGE(TAG, "usb link task creation failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "CDC telemetry ready");
    return ESP_OK;
}
