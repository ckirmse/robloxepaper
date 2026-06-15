#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "history/player_history.h"

class GraphScreen {
public:
    GraphScreen() = default;
    ~GraphScreen() = default;

    GraphScreen(const GraphScreen &) = delete;
    GraphScreen & operator=(const GraphScreen &) = delete;

    void init();

    lv_obj_t * lvglScreen() const { return m_screen; }

    void setGameName(const char * name);
    void setCurrentCount(int32_t count);
    void setHistory(const PlayerHistory & history);
    void setError(bool has_error);

private:
    lv_obj_t * m_screen = nullptr;
    lv_obj_t * m_header = nullptr;
    lv_obj_t * m_name_label = nullptr;
    lv_obj_t * m_error_icon = nullptr;
    lv_obj_t * m_count_label = nullptr;
    lv_obj_t * m_peak_label = nullptr;
    lv_obj_t * m_chart_area = nullptr;
    lv_obj_t * m_oldest_time_label = nullptr;
    lv_obj_t * m_newest_time_label = nullptr;

    const PlayerHistory * m_history = nullptr;

    static void chartDrawCb(lv_event_t * event);
};
