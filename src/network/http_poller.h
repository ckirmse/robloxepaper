#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "network/wifi.h"

struct HttpResult {
    bool success;
    bool votes_success;
    int player_count;
    int visits;
    int up_votes;
    int down_votes;
    char game_name[64];   // ASCII-only, non-printable and brackets stripped
    char game_updated[12]; // "YYYY-MM-DD"
};

class HttpPoller {
public:
    HttpPoller() = default;
    ~HttpPoller() = default;

    HttpPoller(const HttpPoller &) = delete;
    HttpPoller & operator=(const HttpPoller &) = delete;

    void init(Wifi & wifi, QueueHandle_t result_queue);
    void start();

    void triggerNow();

    void setPollIntervalSec(int seconds);
    int getPollIntervalSec() const { return m_poll_interval_sec; }

private:
    Wifi * m_wifi = nullptr;
    QueueHandle_t m_result_queue = nullptr;
    int m_poll_interval_sec = 60;
    volatile bool m_force_fetch = false;

    friend void httpPollerTask(void * arg);
    void run();
    bool fetchGames(HttpResult & result);
    bool fetchVotes(HttpResult & result);
};
