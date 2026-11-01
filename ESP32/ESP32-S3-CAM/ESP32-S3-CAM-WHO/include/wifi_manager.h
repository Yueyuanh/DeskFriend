#pragma once

#include "esp_err.h"

// Brings up netif + event loop + Wi-Fi STA with the static IP from
// include/app_config.h. Connection status is pushed into app_state, so this
// module never blocks a task waiting for an association.
esp_err_t wifi_manager_begin();
