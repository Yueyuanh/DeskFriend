#include "wifi_manager.h"

#include "app_config.h"
#include "app_state.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

#include <cstring>

namespace {

constexpr char TAG[] = "deskfriend_wifi";
esp_netif_t *s_sta_netif = nullptr;

void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        state_set_wifi(false, "0.0.0.0");
        const auto *info = static_cast<const wifi_event_sta_disconnected_t *>(event_data);
        ESP_LOGW(TAG, "Wi-Fi disconnected (reason %d), retrying; status LED stays red",
                 info ? info->reason : -1);
        // Keep retrying; the event loop task is blocked here only for the call.
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        esp_netif_ip_info_t ip_info{};
        ESP_ERROR_CHECK(esp_netif_get_ip_info(s_sta_netif, &ip_info));
        char ip[16]{};
        esp_ip4addr_ntoa(&ip_info.ip, ip, sizeof(ip));
        state_set_wifi(true, ip);
        // Printed on every (re)connect so the address is easy to find on a
        // serial monitor; the status LED turns green at the same moment.
        ESP_LOGI(TAG, "Wi-Fi connected, IP: %s", ip);
        ESP_LOGI(TAG, "Web UI: http://%s/", ip);
    }
}

} // namespace

esp_err_t wifi_manager_begin()
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        return err;
    }
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info{};
    ip4_addr_t parsed{};
    ip4addr_aton(DF_WIFI_STATIC_IP, &parsed);
    ip_info.ip.addr = parsed.addr;
    ip4addr_aton(DF_WIFI_GATEWAY, &parsed);
    ip_info.gw.addr = parsed.addr;
    ip4addr_aton(DF_WIFI_NETMASK, &parsed);
    ip_info.netmask.addr = parsed.addr;
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(s_sta_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_sta_netif, &ip_info));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    wifi_config_t station_cfg = {};
    std::strncpy(reinterpret_cast<char *>(station_cfg.sta.ssid), DF_WIFI_SSID, sizeof(station_cfg.sta.ssid));
    std::strncpy(reinterpret_cast<char *>(station_cfg.sta.password), DF_WIFI_PASSWORD,
                 sizeof(station_cfg.sta.password));
    station_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "STA started, ssid=%s, static ip=%s", DF_WIFI_SSID, DF_WIFI_STATIC_IP);
    return ESP_OK;
}
