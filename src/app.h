#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "display/epaper.h"
#include "display/lvgl_display.h"
#include "network/wifi.h"
#include "network/http_poller.h"
#include "input/buttons.h"
#include "screens/player_count.h"
#include "screens/graph_screen.h"
#include "history/player_history.h"
#include "games.h"

enum class ViewId {
    STATS = 0,
    GRAPH = 1,
};

class App {
public:
    App();
    ~App();

    App(const App &) = delete;
    App & operator=(const App &) = delete;

    void init();
    void poll();

private:
    static constexpr int POLL_INTERVAL_SEC = 60;

    struct GameState {
        GameStats last = {};
        PlayerHistory history;
    };

    Epaper m_epaper;
    LvglDisplay m_lvgl_display;
    Wifi m_wifi;
    HttpPoller m_http_poller;
    Buttons m_buttons;
    PlayerCountScreen m_stats_screen;
    GraphScreen m_graph_screen;

    QueueHandle_t m_http_result_queue = nullptr;
    QueueHandle_t m_button_queue = nullptr;
    QueueSetHandle_t m_queue_set = nullptr;

    ViewId m_active_view = ViewId::STATS;
    GameState m_games[GAME_COUNT];
    int m_active_game = 0;
    bool m_last_success = true;
    char m_last_fetch_time[12] = {};
    lv_timer_t * m_rotate_timer = nullptr;

    void handleButton(const ButtonEvent & event);
    void handleHttpResult(const HttpResult & result);
    void cycleView();
    void showGame(int index);
    void nextGame();
    void prevGame();
    static void rotateTimerCb(lv_timer_t * timer);
};
