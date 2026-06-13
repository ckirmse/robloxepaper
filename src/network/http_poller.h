#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "network/wifi.h"

struct HttpResult {
    bool success;
    bool votes_success;
    int32_t player_count;
    int32_t visits;
    int32_t up_votes;
    int32_t down_votes;
    char game_name[64];   // ASCII-only, non-printable and brackets stripped
    char game_updated[12]; // "YYYY-MM-DD"
};

class HttpPoller {
public:
    HttpPoller();
    ~HttpPoller();

    HttpPoller(const HttpPoller &) = delete;
    HttpPoller & operator=(const HttpPoller &) = delete;

    void init(Wifi & wifi, QueueHandle_t result_queue);
    void start();

    void triggerNow();

    void setPollIntervalSec(int seconds);
    int getPollIntervalSec() const { return m_poll_interval_sec; }

private:
    Wifi * m_wifi;
    QueueHandle_t m_result_queue;
    int m_poll_interval_sec;
    volatile bool m_force_fetch;

    friend void httpPollerTask(void * arg);
    void run();
    bool fetchGames(HttpResult & result);
    bool fetchVotes(HttpResult & result);
};
