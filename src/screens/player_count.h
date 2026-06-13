#pragma once

#include <stdint.h>

#include "lvgl.h"

class PlayerCountScreen {
public:
    PlayerCountScreen();
    ~PlayerCountScreen();

    PlayerCountScreen(const PlayerCountScreen &) = delete;
    PlayerCountScreen & operator=(const PlayerCountScreen &) = delete;

    void init();

    void setGameName(const char * name);
    void setCount(int32_t count);
    void setVisits(int32_t visits);
    void setUpvotes(int32_t up, int32_t down);
    void setGameUpdated(const char * date);  // "YYYY-MM-DD"
    void setFetchTime(const char * hhmm);   // "HH:MM"
    void setError(bool has_error);

private:
    lv_obj_t * m_screen;
    lv_obj_t * m_name_label;
    lv_obj_t * m_div1;
    lv_obj_t * m_count_label;
    lv_obj_t * m_players_label;
    lv_obj_t * m_div2;
    lv_obj_t * m_visits_label;
    lv_obj_t * m_upvotes_label;
    lv_obj_t * m_div3;
    lv_obj_t * m_game_update_label;
    lv_obj_t * m_fetch_time_label;
    lv_obj_t * m_error_icon;
};
