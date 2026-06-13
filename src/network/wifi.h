#pragma once

#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"

class Wifi {
public:
    Wifi();
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
    bool m_is_connected;
    int m_retry_count;

    SemaphoreHandle_t m_connected_semaphore;

    friend void wifiEventHandler(void * arg, esp_event_base_t event_base,
                                 int32_t event_id, void * event_data);
    void connect();
};
