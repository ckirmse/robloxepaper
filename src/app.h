#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "display/epaper.h"
#include "display/lvgl_display.h"
#include "network/wifi.h"
#include "network/http_poller.h"
#include "input/buttons.h"
#include "screens/player_count.h"

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
    PlayerCountScreen m_screen;

    QueueHandle_t m_http_result_queue;
    QueueHandle_t m_button_queue;

    int m_poll_interval_idx;

    void handleButton(const ButtonEvent & event);
    void handleHttpResult(const HttpResult & result);
};
