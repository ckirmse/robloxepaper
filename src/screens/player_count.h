#pragma once

#include <stdint.h>

#include "lvgl.h"

class PlayerCountScreen {
public:
    PlayerCountScreen() = default;
    ~PlayerCountScreen() = default;

    PlayerCountScreen(const PlayerCountScreen &) = delete;
    PlayerCountScreen & operator=(const PlayerCountScreen &) = delete;

    void init();

    void setGameName(const char * name);
    void setCount(int count);
    void setVisits(int visits);
    void setUpvotes(int up, int down);
    void setGameUpdated(const char * date);  // "YYYY-MM-DD"
    void setFetchTime(const char * hhmm);   // "HH:MM"
    void setError(bool has_error);

private:
    lv_obj_t * m_screen = nullptr;
    lv_obj_t * m_header = nullptr;   // black top bar (replaces m_div1)
    lv_obj_t * m_name_label = nullptr;
    lv_obj_t * m_count_label = nullptr;
    lv_obj_t * m_players_label = nullptr;
    lv_obj_t * m_div2 = nullptr;
    lv_obj_t * m_visits_label = nullptr;
    lv_obj_t * m_upvotes_label = nullptr;
    lv_obj_t * m_div3 = nullptr;
    lv_obj_t * m_game_update_label = nullptr;
    lv_obj_t * m_fetch_time_label = nullptr;
    lv_obj_t * m_error_icon = nullptr;
};
