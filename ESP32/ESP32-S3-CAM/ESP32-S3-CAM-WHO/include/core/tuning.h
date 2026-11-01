#pragma once

#include "esp_err.h"

#include <cstdint>

// Runtime-adjustable face detection / recognition thresholds.
// The web task writes them (GET /api/tuning), the vision task reads them and
// re-applies the detection threshold when tuning_revision() changes.
// Values live in NVS so a reboot keeps whatever was tuned on the page.
struct TuningParams {
    // Score threshold for both detector stages (MSR candidates + MNP refine).
    // ESP-DL's default is 0.5; lower finds off-angle or small faces.
    float detect_score = 0.40f;
    // Cosine similarity needed to accept a match. HumanFaceRecognizer's
    // hard-coded 0.5 is too strict for faces that are not square to the lens.
    float recognize_sim = 0.50f;
};

// Loads saved values (defaults when NVS has none). Call after nvs_flash_init().
esp_err_t tuning_begin();

// Safe to call from any task. The floats are 32-bit aligned, so a read either
// sees the old or the new value, never a torn one.
TuningParams tuning_get();

// Clamps to the supported ranges, stores, bumps the revision and writes NVS.
// Returns ESP_OK even if only the NVS write failed; the value still applies
// until the next reboot.
esp_err_t tuning_set(const TuningParams &params);

// Restores the defaults and saves them; also bumps the revision.
void tuning_reset();

// Increments on every tuning_set()/tuning_reset(), so the vision task knows
// when to push a new detection threshold into the model.
uint32_t tuning_revision();
