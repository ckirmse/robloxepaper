#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "network/wifi.h"
#include "games.h"

// Stats for one game, as parsed from the combined API responses
struct GameStats {
    bool success;          // games endpoint parsed for this game
    bool votes_success;    // votes endpoint parsed for this game
    int player_count;
    int visits;
    int up_votes;
    int down_votes;
    char game_name[64];    // ASCII-only, non-printable and brackets stripped
    char game_updated[12]; // "YYYY-MM-DD"
};

// One poll cycle's results for every game in GAME_UNIVERSE_IDS (same order)
struct HttpResult {
    bool success;          // both GETs succeeded and every game parsed
    GameStats games[GAME_COUNT];
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
    bool m_time_synced = false;
    SemaphoreHandle_t m_wake_sem = nullptr;  // given on triggerNow() or interval change
    char m_games_url[128] = {};
    char m_votes_url[128] = {};

    friend void httpPollerTask(void * arg);
    void run();
    void buildUrls();
    bool fetchGames(HttpResult & result);
    bool fetchVotes(HttpResult & result);
};
