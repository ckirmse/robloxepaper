#include "app.h"

#include <time.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "log.h"

static const char * TAG = "APP";

// US Pacific (PST=UTC-8 with PDT daylight saving)
static constexpr const char * TZ_PACIFIC = "PST8PDT,M3.2.0,M11.1.0";

const int App::POLL_INTERVALS[App::POLL_INTERVAL_COUNT] = {60, 120, 300, 1800};

App::App() :
    m_lvgl_display(m_epaper) {
    m_http_result_queue = xQueueCreate(1, sizeof(HttpResult));
    m_button_queue = xQueueCreate(8, sizeof(ButtonEvent));
    m_last_result.success = true;  // first fetch sets error icon only on actual failure
}

App::~App() {
    vQueueDelete(m_http_result_queue);
    vQueueDelete(m_button_queue);
}

void App::init() {
    lprintf(TAG, "Initializing app");

    m_epaper.init();
    m_lvgl_display.init();
    m_stats_screen.init();
    m_graph_screen.init();

    m_history.load();

    lv_screen_load(m_stats_screen.lvglScreen());

    m_wifi.init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

    m_http_poller.init(m_wifi, m_http_result_queue);
    m_http_poller.setPollIntervalSec(POLL_INTERVALS[m_poll_interval_index]);
    m_http_poller.start();

    m_buttons.init(m_button_queue);

    setenv("TZ", TZ_PACIFIC, 1);
    tzset();

    lprintf(TAG, "App initialized, poll interval %ds", POLL_INTERVALS[m_poll_interval_index]);
}

void App::poll() {
    ButtonEvent btn_event;
    while (xQueueReceive(m_button_queue, &btn_event, 0) == pdTRUE) {
        handleButton(btn_event);
    }

    HttpResult http_result;
    while (xQueueReceive(m_http_result_queue, &http_result, 0) == pdTRUE) {
        handleHttpResult(http_result);
    }

    m_lvgl_display.tick();
}

void App::cycleView() {
    if (m_active_view == ViewId::STATS) {
        m_active_view = ViewId::GRAPH;
        lv_screen_load(m_graph_screen.lvglScreen());
    } else {
        m_active_view = ViewId::STATS;
        lv_screen_load(m_stats_screen.lvglScreen());
    }
}

void App::handleButton(const ButtonEvent & event) {
    switch (event.id) {
    case ButtonId::HOME:
        lprintf(TAG, "HOME pressed — forcing fetch");
        m_http_poller.triggerNow();
        break;

    case ButtonId::OK:
        lprintf(TAG, "OK pressed — cycling view");
        cycleView();
        break;

    case ButtonId::UP: {
        if (m_poll_interval_index < POLL_INTERVAL_COUNT - 1) {
            m_poll_interval_index++;
        }
        int interval = POLL_INTERVALS[m_poll_interval_index];
        m_http_poller.setPollIntervalSec(interval);
        lprintf(TAG, "UP pressed — poll interval → %ds", interval);
        break;
    }

    case ButtonId::DOWN: {
        if (m_poll_interval_index > 0) {
            m_poll_interval_index--;
        }
        int interval = POLL_INTERVALS[m_poll_interval_index];
        m_http_poller.setPollIntervalSec(interval);
        lprintf(TAG, "DOWN pressed — poll interval → %ds", interval);
        break;
    }

    case ButtonId::EXIT:
        lprintf(TAG, "EXIT pressed — showing stale indicator then sleeping");
        m_active_view = ViewId::STATS;
        lv_screen_load(m_stats_screen.lvglScreen());
        m_stats_screen.setError(true);
        m_lvgl_display.tick();
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);
        esp_deep_sleep_start();
        break;

    default:
        break;
    }
}

void App::handleHttpResult(const HttpResult & result) {
    if (result.success != m_last_result.success) {
        m_stats_screen.setError(!result.success);
        m_graph_screen.setError(!result.success);
    }

    if (!result.success) {
        eprintf(TAG, "HTTP fetch failed");
        m_last_result.success = false;
        return;
    }

    lprintf(TAG, "Got player count: %d, visits: %d",
            result.player_count, result.visits);

    char fetch_time[12] = "12:00 AM";
    time_t now_ts = time(nullptr);
    if (now_ts > 1000000) {  // SNTP has synced (epoch > year 1970+11days)
        struct tm now_tm = {};
        localtime_r(&now_ts, &now_tm);
        strftime(fetch_time, sizeof(fetch_time), "%l:%M %p", &now_tm);
    }

    if (strcmp(result.game_name, m_last_result.game_name) != 0) {
        m_stats_screen.setGameName(result.game_name);
    }
    if (result.player_count != m_last_result.player_count) {
        m_stats_screen.setCount(result.player_count);
    }
    if (result.visits != m_last_result.visits) {
        m_stats_screen.setVisits(result.visits);
    }
    if (result.up_votes != m_last_result.up_votes || result.down_votes != m_last_result.down_votes) {
        m_stats_screen.setUpvotes(result.up_votes, result.down_votes);
    }
    if (strcmp(result.game_updated, m_last_result.game_updated) != 0) {
        m_stats_screen.setGameUpdated(result.game_updated);
    }
    if (strcmp(fetch_time, m_last_fetch_time) != 0) {
        m_stats_screen.setFetchTime(fetch_time);
        strcpy(m_last_fetch_time, fetch_time);
    }

    m_history.push(result.player_count, (int32_t)now_ts);
    if (strcmp(result.game_name, m_last_result.game_name) != 0) {
        m_graph_screen.setGameName(result.game_name);
    }
    if (result.player_count != m_last_result.player_count) {
        m_graph_screen.setCurrentCount(result.player_count);
    }
    m_graph_screen.setHistory(m_history);

    m_last_result = result;
}
