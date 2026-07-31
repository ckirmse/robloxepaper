#include "network/wifi.h"

#include <cinttypes>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "log.h"

static const char * TAG = "WIFI";

// Reconnect backoff: 500ms doubling per attempt, capped at 60s. Never give up —
// the router may come back hours later and we must recover without a reboot.
static constexpr int64_t RECONNECT_BASE_DELAY_MS = 500;
static constexpr int64_t RECONNECT_MAX_DELAY_MS = 60 * 1000;

// Modem power save: MAX saves the most; fall back to WIFI_PS_MIN_MODEM if the
// AP drops the connection ("bcn timeout" in logs)
static constexpr wifi_ps_type_t POWER_SAVE_MODE = WIFI_PS_MAX_MODEM;
static constexpr int LISTEN_INTERVAL = 3;

void wifiEventHandler(void * arg, esp_event_base_t event_base,
                      int32_t event_id, void * event_data) {
    Wifi * wifi = static_cast<Wifi *>(arg);

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            lprintf(TAG, "WiFi station started, connecting...");
            wifi->connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi->m_is_connected = false;
            int64_t delay_ms = RECONNECT_BASE_DELAY_MS << (wifi->m_retry_count < 8 ? wifi->m_retry_count : 8);
            if (delay_ms > RECONNECT_MAX_DELAY_MS) {
                delay_ms = RECONNECT_MAX_DELAY_MS;
            }
            wifi->m_retry_count++;
            lprintf(TAG, "Disconnected, retry %d in %" PRId64 " ms", wifi->m_retry_count, delay_ms);
            esp_timer_stop(wifi->m_reconnect_timer);
            esp_timer_start_once(wifi->m_reconnect_timer, delay_ms * 1000);
            break;
        }

        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t * event = static_cast<ip_event_got_ip_t *>(event_data);
            lprintf(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            wifi->m_is_connected = true;
            wifi->m_retry_count = 0;
            esp_timer_stop(wifi->m_reconnect_timer);
            xSemaphoreGive(wifi->m_connected_semaphore);
        }
    }
}

void reconnectTimerCallback(void * arg) {
    static_cast<Wifi *>(arg)->connect();
}

Wifi::~Wifi() {
    vSemaphoreDelete(m_connected_semaphore);
}

void Wifi::init(const char * ssid, const char * password) {
    m_ssid = ssid;
    m_password = password;

    esp_log_level_set("wifi", ESP_LOG_WARN);

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = reconnectTimerCallback;
    timer_args.arg = this;
    timer_args.name = "wifi_reconnect";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &m_reconnect_timer));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifiEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifiEventHandler, this));

    wifi_config_t wifi_cfg = {};
    strncpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid), m_ssid.c_str(),
            sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy(reinterpret_cast<char *>(wifi_cfg.sta.password), m_password.c_str(),
            sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.listen_interval = LISTEN_INTERVAL;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(POWER_SAVE_MODE));

    lprintf(TAG, "WiFi initialized, connecting to \"%s\"", m_ssid.c_str());
}

void Wifi::connect() {
    esp_wifi_connect();
}

bool Wifi::isConnected() const {
    return m_is_connected;
}

bool Wifi::waitForConnection(int timeout_ms) {
    return xSemaphoreTake(m_connected_semaphore, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
