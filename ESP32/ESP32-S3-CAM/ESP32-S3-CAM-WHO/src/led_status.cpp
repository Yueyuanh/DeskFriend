#include "led_status.h"

#include "app_state.h"

#include "driver/gpio.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>

namespace {

constexpr char TAG[] = "deskfriend_led";

// The GOOUUU ESP32-S3-CAM board wires its RGB pixel to GPIO48, GRB order.
constexpr gpio_num_t LED_GPIO = GPIO_NUM_48;
constexpr uint32_t RMT_RESOLUTION_HZ = 10 * 1000 * 1000;  // 0.1 us per tick
constexpr uint32_t BLINK_MS = 500;
constexpr uint32_t TICK_MS = 20;
constexpr uint8_t BRIGHTNESS = 64;  // same level as ESP32-S3-Dual

rmt_channel_handle_t s_channel = nullptr;
rmt_encoder_handle_t s_encoder = nullptr;

// One WS2812 bit is a high pulse followed by a low gap; the bytes encoder turns
// each bit of the 3-byte GRB value into exactly that symbol.
esp_err_t led_send(uint8_t green, uint8_t red, uint8_t blue)
{
    const uint8_t grb[3] = {green, red, blue};
    rmt_transmit_config_t tx = {};
    tx.loop_count = 1;
    return rmt_transmit(s_channel, s_encoder, grb, sizeof(grb), &tx);
}

void led_task(void *)
{
    bool blink_on = false;
    uint32_t last_toggle = 0;
    bool have_color = false;
    uint8_t cur_r = 0, cur_g = 0, cur_b = 0;

    for (;;) {
        uint8_t r = 0, g = 0, b = 0;
        if (state_wifi_connected()) {
            g = BRIGHTNESS;  // connected: solid green
        } else {
            const uint32_t now = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
            if (now - last_toggle >= BLINK_MS) {
                last_toggle = now;
                blink_on = !blink_on;
            }
            r = blink_on ? BRIGHTNESS : 0;  // no IP yet: red blink
        }

        if (!have_color || r != cur_r || g != cur_g || b != cur_b) {
            const esp_err_t err = led_send(g, r, b);
            if (err == ESP_OK) {
                cur_r = r;
                cur_g = g;
                cur_b = b;
                have_color = true;
            } else {
                ESP_LOGW(TAG, "WS2812 transmit failed: %s", esp_err_to_name(err));
                have_color = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

} // namespace

esp_err_t led_status_start()
{
    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = LED_GPIO;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = RMT_RESOLUTION_HZ;
    tx_config.mem_block_symbols = 64;
    tx_config.trans_queue_depth = 4;
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_config, &s_channel), TAG, "rmt_new_tx_channel");

    // T0H 0.4 us / T0L 0.9 us, T1H 0.8 us / T1L 0.5 us, 1.3 us period.
    rmt_bytes_encoder_config_t encoder_config = {};
    encoder_config.bit0.level0 = 1;
    encoder_config.bit0.duration0 = 4;
    encoder_config.bit0.level1 = 0;
    encoder_config.bit0.duration1 = 9;
    encoder_config.bit1.level0 = 1;
    encoder_config.bit1.duration0 = 8;
    encoder_config.bit1.level1 = 0;
    encoder_config.bit1.duration1 = 5;
    encoder_config.flags.msb_first = 1;
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&encoder_config, &s_encoder), TAG, "rmt_new_bytes_encoder");

    ESP_RETURN_ON_ERROR(rmt_enable(s_channel), TAG, "rmt_enable");

    if (xTaskCreatePinnedToCore(led_task, "led_status", 3072, nullptr, 2, nullptr, 0) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "status LED started on GPIO%d (red blink = no Wi-Fi, green = connected)",
             static_cast<int>(LED_GPIO));
    return ESP_OK;
}
