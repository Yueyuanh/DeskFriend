#include "core/tuning.h"

#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr char TAG[] = "deskfriend_tuning";
constexpr char NVS_NAMESPACE[] = "tuning";
constexpr char KEY_DETECT[] = "det_thr";
constexpr char KEY_RECOGNIZE[] = "rec_thr";

// Keep the sliders on the page inside the same limits the firmware enforces.
constexpr float DETECT_MIN = 0.20f;
constexpr float DETECT_MAX = 0.70f;
constexpr float RECOGNIZE_MIN = 0.30f;
constexpr float RECOGNIZE_MAX = 0.90f;

TuningParams s_params;
uint32_t s_revision = 1;

float clamp(float value, float lo, float hi)
{
    if (value < lo) {
        return lo;
    }
    return value > hi ? hi : value;
}

// NVS stores integers, so thresholds are kept as thousandths.
int32_t to_milli(float value)
{
    return static_cast<int32_t>(value * 1000.0f + 0.5f);
}

float from_milli(int32_t value)
{
    return static_cast<float>(value) / 1000.0f;
}

void save()
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_i32(handle, KEY_DETECT, to_milli(s_params.detect_score));
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, KEY_RECOGNIZE, to_milli(s_params.recognize_sim));
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "saving thresholds failed: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
}

} // namespace

esp_err_t tuning_begin()
{
    // READONLY fails with ESP_ERR_NVS_NOT_FOUND before the first save, which is
    // exactly the "use defaults" case.
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        int32_t detect = 0;
        int32_t recognize = 0;
        if (nvs_get_i32(handle, KEY_DETECT, &detect) == ESP_OK) {
            s_params.detect_score = from_milli(detect);
        }
        if (nvs_get_i32(handle, KEY_RECOGNIZE, &recognize) == ESP_OK) {
            s_params.recognize_sim = from_milli(recognize);
        }
        nvs_close(handle);
    }

    s_params.detect_score = clamp(s_params.detect_score, DETECT_MIN, DETECT_MAX);
    s_params.recognize_sim = clamp(s_params.recognize_sim, RECOGNIZE_MIN, RECOGNIZE_MAX);
    ESP_LOGI(TAG, "thresholds: detect %.2f, recognize %.2f", s_params.detect_score,
             s_params.recognize_sim);
    return ESP_OK;
}

TuningParams tuning_get()
{
    return s_params;
}

esp_err_t tuning_set(const TuningParams &params)
{
    s_params.detect_score = clamp(params.detect_score, DETECT_MIN, DETECT_MAX);
    s_params.recognize_sim = clamp(params.recognize_sim, RECOGNIZE_MIN, RECOGNIZE_MAX);
    ++s_revision;
    save();
    ESP_LOGI(TAG, "thresholds -> detect %.2f, recognize %.2f", s_params.detect_score,
             s_params.recognize_sim);
    return ESP_OK;
}

void tuning_reset()
{
    tuning_set(TuningParams{});
}

uint32_t tuning_revision()
{
    return s_revision;
}
