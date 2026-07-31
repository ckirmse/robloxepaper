#pragma once

#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_timer.h"

class Wifi {
public:
    Wifi() = default;
    ~Wifi();

    Wifi(const Wifi &) = delete;
    Wifi & operator=(const Wifi &) = delete;

    void init(const char * ssid, const char * password);

    bool isConnected() const;

    // Blocks until connected or timeout_ms elapses; returns true if connected
    bool waitForConnection(int timeout_ms);

    // Semaphore taken when connected, for use by HTTP poller task
    SemaphoreHandle_t getConnectedSemaphore() { return m_connected_semaphore; }

private:
    std::string m_ssid;
    std::string m_password;
    bool m_is_connected = false;
    int m_retry_count = 0;
    esp_timer_handle_t m_reconnect_timer = nullptr;

    SemaphoreHandle_t m_connected_semaphore = xSemaphoreCreateBinary();

    friend void reconnectTimerCallback(void * arg);
    friend void wifiEventHandler(void * arg, esp_event_base_t event_base,
                                 int32_t event_id, void * event_data);
    void connect();
};
