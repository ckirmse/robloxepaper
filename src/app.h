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
    static const int POLL_INTERVAL_COUNT = 4;
    static const int POLL_INTERVALS[POLL_INTERVAL_COUNT];

    Epaper m_epaper;
    LvglDisplay m_lvgl_display;
    Wifi m_wifi;
    HttpPoller m_http_poller;
    Buttons m_buttons;
    PlayerCountScreen m_stats_screen;
    GraphScreen m_graph_screen;
    PlayerHistory m_history;

    QueueHandle_t m_http_result_queue = nullptr;
    QueueHandle_t m_button_queue = nullptr;

    int m_poll_interval_index = 0;
    ViewId m_active_view = ViewId::STATS;
    HttpResult m_last_result = {};
    char m_last_fetch_time[12] = {};

    void handleButton(const ButtonEvent & event);
    void handleHttpResult(const HttpResult & result);
    void cycleView();
};
