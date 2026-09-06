#include "app.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/gpio.h"

#include "log.h"

static const char * TAG = "APP";

// US Pacific (PST=UTC-8 with PDT daylight saving)
static constexpr const char * TZ_PACIFIC = "PST8PDT,M3.2.0,M11.1.0";

App::App() :
    m_lvgl_display(m_epaper) {
    m_http_result_queue = xQueueCreate(1, sizeof(HttpResult));
    m_button_queue = xQueueCreate(8, sizeof(ButtonEvent));
    m_queue_set = xQueueCreateSet(9);  // button queue (8) + http result queue (1)
    xQueueAddToSet(m_http_result_queue, m_queue_set);
    xQueueAddToSet(m_button_queue, m_queue_set);
}

App::~App() {
    vQueueDelete(m_queue_set);
    vQueueDelete(m_http_result_queue);
    vQueueDelete(m_button_queue);
}

void App::init() {
    lprintf(TAG, "Initializing app");

    esp_pm_config_t pm_config = {};
    pm_config.max_freq_mhz = 80;
    pm_config.min_freq_mhz = 40;
    pm_config.light_sleep_enable = true;
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
    lprintf(TAG, "Power management enabled: DFS 40-80MHz, auto light sleep");

    m_epaper.init();
    m_lvgl_display.init();
    m_stats_screen.init();
    m_graph_screen.init();

    for (int i = 0; i < GAME_COUNT; i++) {
        m_games[i].history.load(i);
    }

    lv_screen_load(m_stats_screen.lvglScreen());

    if (GAME_COUNT > 1) {
        // Runs from lv_timer_handler() in LvglDisplay::tick() on this task
        m_rotate_timer = lv_timer_create(rotateTimerCb, GAME_ROTATE_SEC * 1000, this);
    }

    m_wifi.init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

    m_http_poller.init(m_wifi, m_http_result_queue);
    m_http_poller.setPollIntervalSec(POLL_INTERVAL_SEC);
    m_http_poller.start();

    m_buttons.init(m_button_queue);

    setenv("TZ", TZ_PACIFIC, 1);
    tzset();

    lprintf(TAG, "App initialized, %d games, poll interval %ds, rotate %ds",
            GAME_COUNT, POLL_INTERVAL_SEC, GAME_ROTATE_SEC);
}

void App::poll() {
    uint32_t idle_ms = m_lvgl_display.tick();

    QueueSetMemberHandle_t member = xQueueSelectFromSet(m_queue_set, pdMS_TO_TICKS(idle_ms));
    if (member == m_button_queue) {
        ButtonEvent btn_event;
        if (xQueueReceive(m_button_queue, &btn_event, 0) == pdTRUE) {
            m_buttons.rearm();
            handleButton(btn_event);
        }
    } else if (member == m_http_result_queue) {
        HttpResult http_result;
        if (xQueueReceive(m_http_result_queue, &http_result, 0) == pdTRUE) {
            handleHttpResult(http_result);
        }
    }
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

void App::rotateTimerCb(lv_timer_t * timer) {
    App * self = static_cast<App *>(lv_timer_get_user_data(timer));
    self->nextGame();
}

void App::nextGame() {
    showGame((m_active_game + 1) % GAME_COUNT);
}

void App::prevGame() {
    showGame((m_active_game + GAME_COUNT - 1) % GAME_COUNT);
}

// Push every field of the given game into both screens and restart the dwell timer
void App::showGame(int index) {
    m_active_game = index;
    const GameState & game = m_games[index];
    lprintf(TAG, "Showing game %d (universe %" PRId64 ")", index, GAME_UNIVERSE_IDS[index]);

    m_stats_screen.setGameName(game.last.game_name);
    m_stats_screen.setCount(game.last.player_count);
    m_stats_screen.setVisits(game.last.visits);
    m_stats_screen.setUpvotes(game.last.up_votes, game.last.down_votes);
    m_stats_screen.setGameUpdated(game.last.game_updated);
    m_stats_screen.setFetchTime(m_last_fetch_time);

    m_graph_screen.setGameName(game.last.game_name);
    m_graph_screen.setCurrentCount(game.last.player_count);
    m_graph_screen.setHistory(game.history);

    if (m_rotate_timer) {
        lv_timer_reset(m_rotate_timer);
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

    case ButtonId::UP:
        lprintf(TAG, "UP pressed — next game");
        nextGame();
        break;

    case ButtonId::DOWN:
        lprintf(TAG, "DOWN pressed — previous game");
        prevGame();
        break;

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
    if (result.success != m_last_success) {
        m_stats_screen.setError(!result.success);
        m_graph_screen.setError(!result.success);
    }
    m_last_success = result.success;

    if (!result.success) {
        eprintf(TAG, "HTTP fetch failed");
        return;
    }

    char fetch_time[12] = "12:00 AM";
    time_t now_ts = time(nullptr);
    if (now_ts > 1000000) {  // SNTP has synced (epoch > year 1970+11days)
        struct tm now_tm = {};
        localtime_r(&now_ts, &now_tm);
        strftime(fetch_time, sizeof(fetch_time), "%l:%M %p", &now_tm);
    }

    for (int i = 0; i < GAME_COUNT; i++) {
        const GameStats & fresh = result.games[i];
        GameState & game = m_games[i];
        lprintf(TAG, "Game %d: %d players, %d visits", i, fresh.player_count, fresh.visits);

        game.history.push(fresh.player_count, (int32_t)now_ts);

        if (i == m_active_game) {
            // Only touch labels that changed so partial refresh stays small
            const GameStats & prev = game.last;
            if (strcmp(fresh.game_name, prev.game_name) != 0) {
                m_stats_screen.setGameName(fresh.game_name);
                m_graph_screen.setGameName(fresh.game_name);
            }
            if (fresh.player_count != prev.player_count) {
                m_stats_screen.setCount(fresh.player_count);
                m_graph_screen.setCurrentCount(fresh.player_count);
            }
            if (fresh.visits != prev.visits) {
                m_stats_screen.setVisits(fresh.visits);
            }
            if (fresh.up_votes != prev.up_votes || fresh.down_votes != prev.down_votes) {
                m_stats_screen.setUpvotes(fresh.up_votes, fresh.down_votes);
            }
            if (strcmp(fresh.game_updated, prev.game_updated) != 0) {
                m_stats_screen.setGameUpdated(fresh.game_updated);
            }
            m_graph_screen.setHistory(game.history);
        }

        game.last = fresh;
    }

    if (strcmp(fetch_time, m_last_fetch_time) != 0) {
        m_stats_screen.setFetchTime(fetch_time);
        strcpy(m_last_fetch_time, fetch_time);
    }
}
