#include "screens/player_count.h"

#include <stdio.h>
#include <string.h>

#include "log.h"

static const char * TAG = "SCR";

LV_FONT_DECLARE(lv_font_press_start_2p_80)
LV_FONT_DECLARE(lv_font_builder_sans_semibold_20)
LV_FONT_DECLARE(lv_font_builder_sans_semibold_28)
LV_FONT_DECLARE(lv_font_builder_sans_semibold_16)

// Format a non-negative integer with comma separators into buf.
static void formatNumber(char * buf, size_t buf_size, int val) {
    if (val < 0) {
        snprintf(buf, buf_size, "%d", val);
        return;
    }
    if (val >= 1000000) {
        snprintf(buf, buf_size, "%d,%03d,%03d",
                 val / 1000000,
                 (val / 1000) % 1000,
                 val % 1000);
    } else if (val >= 1000) {
        snprintf(buf, buf_size, "%d,%03d",
                 val / 1000,
                 val % 1000);
    } else {
        snprintf(buf, buf_size, "%d", val);
    }
}

// Parse "YYYY-MM-DD" → "Mmm DD YYYY"
static void formatDate(const char * iso_date, char * out, size_t out_size) {
    static const char * MONTHS[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    int year = 0, month = 0, day = 0;
    if (sscanf(iso_date, "%d-%d-%d", &year, &month, &day) == 3
            && month >= 1 && month <= 12) {
        snprintf(out, out_size, "%s %d %d", MONTHS[month - 1], day, year);
    } else {
        snprintf(out, out_size, "%s", iso_date);
    }
}

static lv_obj_t * makeDivider(lv_obj_t * parent, int y) {
    lv_obj_t * div = lv_obj_create(parent);
    lv_obj_set_size(div, 380, 1);
    lv_obj_set_pos(div, 10, y);
    lv_obj_set_style_bg_color(div, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    lv_obj_set_style_radius(div, 0, 0);
    return div;
}

void PlayerCountScreen::init() {
    // ── Screen ────────────────────────────────────────────────────────────
    m_screen = lv_obj_create(nullptr);
    lv_obj_set_size(m_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(m_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_screen, 0, 0);
    lv_obj_set_style_pad_all(m_screen, 0, 0);

    // ── Inverted header bar (black background, full width) ────────────────
    m_header = lv_obj_create(m_screen);
    lv_obj_set_size(m_header, 400, 54);
    lv_obj_set_pos(m_header, 0, 0);
    lv_obj_set_style_bg_color(m_header, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_header, 0, 0);
    lv_obj_set_style_pad_all(m_header, 0, 0);
    lv_obj_set_style_radius(m_header, 0, 0);

    // ── Game name (white text in black header, centered) ──────────────────
    m_name_label = lv_label_create(m_header);
    lv_obj_set_width(m_name_label, 340);  // leave room for error icon
    lv_label_set_long_mode(m_name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(m_name_label, &lv_font_builder_sans_semibold_28, 0);
    lv_obj_set_style_text_color(m_name_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(m_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(m_name_label, "...");
    lv_obj_align(m_name_label, LV_ALIGN_CENTER, 0, 0);

    // ── Error icon (top-right, white box on black header) ─────────────────
    m_error_icon = lv_obj_create(m_header);
    lv_obj_set_size(m_error_icon, 26, 26);
    lv_obj_align(m_error_icon, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_bg_color(m_error_icon, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_error_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(m_error_icon, lv_color_white(), 0);
    lv_obj_set_style_border_width(m_error_icon, 1, 0);
    lv_obj_set_style_radius(m_error_icon, 0, 0);
    lv_obj_set_style_pad_all(m_error_icon, 0, 0);
    lv_obj_t * excl = lv_label_create(m_error_icon);
    lv_label_set_text(excl, "!");
    lv_obj_set_style_text_font(excl, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(excl, lv_color_black(), 0);
    lv_obj_center(excl);
    lv_obj_add_flag(m_error_icon, LV_OBJ_FLAG_HIDDEN);

    // ── Player count (PressStart2P 80px, hero zone center) ────────────────
    m_count_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_count_label, &lv_font_press_start_2p_80, 0);
    lv_obj_set_style_text_color(m_count_label, lv_color_black(), 0);
    lv_label_set_text(m_count_label, "--");
    lv_obj_align(m_count_label, LV_ALIGN_TOP_MID, 0, 92);

    // ── "players online" label ─────────────────────────────────────────────
    m_players_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_players_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_players_label, lv_color_black(), 0);
    lv_label_set_text(m_players_label, "players online");
    lv_obj_align(m_players_label, LV_ALIGN_TOP_MID, 0, 182);

    // ── Divider 2 ─────────────────────────────────────────────────────────
    m_div2 = makeDivider(m_screen, 210);

    // ── Stats row: visits (left) | upvotes (right) ────────────────────────
    m_visits_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_visits_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_visits_label, lv_color_black(), 0);
    lv_label_set_text(m_visits_label, "--");
    lv_obj_align(m_visits_label, LV_ALIGN_TOP_LEFT, 10, 225);

    m_upvotes_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_upvotes_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_upvotes_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(m_upvotes_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(m_upvotes_label, "--");
    lv_obj_align(m_upvotes_label, LV_ALIGN_TOP_RIGHT, -10, 225);

    // ── Divider 3 ─────────────────────────────────────────────────────────
    m_div3 = makeDivider(m_screen, 252);

    // ── Footer row: game date (left) | fetch time (right) ─────────────────
    m_game_update_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_game_update_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_game_update_label, lv_color_black(), 0);
    lv_label_set_text(m_game_update_label, "Game: --");
    lv_obj_align(m_game_update_label, LV_ALIGN_TOP_LEFT, 10, 267);

    m_fetch_time_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_fetch_time_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_fetch_time_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(m_fetch_time_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(m_fetch_time_label, "Fetched: --");
    lv_obj_align(m_fetch_time_label, LV_ALIGN_TOP_RIGHT, -10, 267);

    lv_screen_load(m_screen);
    lprintf(TAG, "Screen initialized");
}

void PlayerCountScreen::setGameName(const char * name) {
    lv_label_set_text(m_name_label, name);
    lv_obj_align(m_name_label, LV_ALIGN_CENTER, 0, 0);
}

void PlayerCountScreen::setCount(int count) {
    char buf[24];
    formatNumber(buf, sizeof(buf), count);
    lv_label_set_text(m_count_label, buf);
    lv_obj_align(m_count_label, LV_ALIGN_TOP_MID, 0, 92);
}

void PlayerCountScreen::setVisits(int visits) {
    char num[20];
    formatNumber(num, sizeof(num), visits);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s visits", num);
    lv_label_set_text(m_visits_label, buf);
    lv_obj_align(m_visits_label, LV_ALIGN_TOP_LEFT, 10, 225);
}

void PlayerCountScreen::setUpvotes(int up, int down) {
    char buf[32];
    int total = up + down;
    if (total > 0) {
        char num[16];
        formatNumber(num, sizeof(num), up);
        int pct = (int)(up * 100 / total);
        snprintf(buf, sizeof(buf), "%s likes (%d%%)", num, pct);
    } else {
        snprintf(buf, sizeof(buf), "-- up");
    }
    lv_label_set_text(m_upvotes_label, buf);
    lv_obj_align(m_upvotes_label, LV_ALIGN_TOP_RIGHT, -10, 225);
}

void PlayerCountScreen::setGameUpdated(const char * date) {
    char formatted[16];  // "Mmm DD YYYY" = 11 chars max
    formatDate(date, formatted, sizeof(formatted));
    char buf[40];
    snprintf(buf, sizeof(buf), "Game Updated: %s", formatted);
    lv_label_set_text(m_game_update_label, buf);
    lv_obj_align(m_game_update_label, LV_ALIGN_TOP_LEFT, 10, 267);
}

void PlayerCountScreen::setFetchTime(const char * hhmm) {
    char buf[32];
    snprintf(buf, sizeof(buf), "As of %s PDT", hhmm);
    lv_label_set_text(m_fetch_time_label, buf);
    lv_obj_align(m_fetch_time_label, LV_ALIGN_TOP_RIGHT, -10, 267);
}

void PlayerCountScreen::setError(bool has_error) {
    if (has_error) {
        lv_obj_clear_flag(m_error_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m_error_icon, LV_OBJ_FLAG_HIDDEN);
    }
}
