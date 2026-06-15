#include "screens/graph_screen.h"

#include <stdio.h>
#include <time.h>

#include "log.h"

static const char * TAG = "GSCR";

LV_FONT_DECLARE(lv_font_builder_sans_semibold_20)
LV_FONT_DECLARE(lv_font_builder_sans_semibold_28)
LV_FONT_DECLARE(lv_font_builder_sans_semibold_16)

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

static void formatTime(int32_t timestamp, char * buf, size_t buf_size) {
    if (timestamp <= 1000000) {
        snprintf(buf, buf_size, "--:--");
        return;
    }
    time_t t = (time_t)timestamp;
    struct tm tm_val = {};
    localtime_r(&t, &tm_val);
    strftime(buf, buf_size, "%l:%M %p", &tm_val);
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

void GraphScreen::init() {
    // ── Screen ────────────────────────────────────────────────────────────
    m_screen = lv_obj_create(nullptr);
    lv_obj_set_size(m_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(m_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_screen, 0, 0);
    lv_obj_set_style_pad_all(m_screen, 0, 0);

    // ── Inverted header bar ───────────────────────────────────────────────
    m_header = lv_obj_create(m_screen);
    lv_obj_set_size(m_header, 400, 54);
    lv_obj_set_pos(m_header, 0, 0);
    lv_obj_set_style_bg_color(m_header, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(m_header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_header, 0, 0);
    lv_obj_set_style_pad_all(m_header, 0, 0);
    lv_obj_set_style_radius(m_header, 0, 0);

    // ── Game name ─────────────────────────────────────────────────────────
    m_name_label = lv_label_create(m_header);
    lv_obj_set_width(m_name_label, 340);
    lv_label_set_long_mode(m_name_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(m_name_label, &lv_font_builder_sans_semibold_28, 0);
    lv_obj_set_style_text_color(m_name_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(m_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(m_name_label, "...");
    lv_obj_align(m_name_label, LV_ALIGN_CENTER, 0, 0);

    // ── Error icon ────────────────────────────────────────────────────────
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

    // ── Info row: current count (left) | peak (right) ────────────────────
    m_count_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_count_label, &lv_font_builder_sans_semibold_20, 0);
    lv_obj_set_style_text_color(m_count_label, lv_color_black(), 0);
    lv_label_set_text(m_count_label, "-- players");
    lv_obj_align(m_count_label, LV_ALIGN_TOP_LEFT, 10, 60);

    m_peak_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_peak_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_peak_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(m_peak_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(m_peak_label, "Peak: --");
    lv_obj_align(m_peak_label, LV_ALIGN_TOP_RIGHT, -10, 63);

    // ── Divider below info row ────────────────────────────────────────────
    makeDivider(m_screen, 82);

    // ── Chart area (bars drawn via event callback) ────────────────────────
    m_chart_area = lv_obj_create(m_screen);
    lv_obj_set_size(m_chart_area, 360, 186);
    lv_obj_set_pos(m_chart_area, 10, 84);
    lv_obj_set_style_bg_color(m_chart_area, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(m_chart_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_chart_area, 0, 0);
    lv_obj_set_style_pad_all(m_chart_area, 0, 0);
    lv_obj_set_style_radius(m_chart_area, 0, 0);
    lv_obj_add_event_cb(m_chart_area, chartDrawCb, LV_EVENT_DRAW_MAIN, this);

    // ── Baseline divider at bottom of chart ───────────────────────────────
    makeDivider(m_screen, 270);

    // ── Time labels: oldest (left) | newest (right) ───────────────────────
    m_oldest_time_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_oldest_time_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_oldest_time_label, lv_color_black(), 0);
    lv_label_set_text(m_oldest_time_label, "--:--");
    lv_obj_align(m_oldest_time_label, LV_ALIGN_TOP_LEFT, 10, 278);

    m_newest_time_label = lv_label_create(m_screen);
    lv_obj_set_style_text_font(m_newest_time_label, &lv_font_builder_sans_semibold_16, 0);
    lv_obj_set_style_text_color(m_newest_time_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(m_newest_time_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(m_newest_time_label, "--:--");
    lv_obj_align(m_newest_time_label, LV_ALIGN_TOP_RIGHT, -10, 278);

    lprintf(TAG, "Graph screen initialized");
}

void GraphScreen::setGameName(const char * name) {
    lv_label_set_text(m_name_label, name);
    lv_obj_align(m_name_label, LV_ALIGN_CENTER, 0, 0);
}

void GraphScreen::setCurrentCount(int32_t count) {
    char num[20];
    formatNumber(num, sizeof(num), (int)count);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s players", num);
    lv_label_set_text(m_count_label, buf);
}

void GraphScreen::setHistory(const PlayerHistory & history) {
    m_history = &history;

    char peak_buf[28];
    int32_t peak_count = history.peak();
    if (peak_count > 0) {
        char num[20];
        formatNumber(num, sizeof(num), (int)peak_count);
        snprintf(peak_buf, sizeof(peak_buf), "Peak: %s", num);
    } else {
        snprintf(peak_buf, sizeof(peak_buf), "Peak: --");
    }
    lv_label_set_text(m_peak_label, peak_buf);

    char oldest_buf[16];
    char newest_buf[16];
    if (history.count() > 0) {
        formatTime(history.at(0).timestamp, oldest_buf, sizeof(oldest_buf));
        formatTime(history.at(history.count() - 1).timestamp, newest_buf, sizeof(newest_buf));
    } else {
        snprintf(oldest_buf, sizeof(oldest_buf), "--:--");
        snprintf(newest_buf, sizeof(newest_buf), "--:--");
    }
    lv_label_set_text(m_oldest_time_label, oldest_buf);
    lv_label_set_text(m_newest_time_label, newest_buf);

    lv_obj_invalidate(m_chart_area);
}

void GraphScreen::setError(bool has_error) {
    if (has_error) {
        lv_obj_clear_flag(m_error_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(m_error_icon, LV_OBJ_FLAG_HIDDEN);
    }
}

void GraphScreen::chartDrawCb(lv_event_t * event) {
    GraphScreen * self = static_cast<GraphScreen *>(lv_event_get_user_data(event));
    if (self->m_history == nullptr || self->m_history->count() == 0) {
        return;
    }

    int32_t max_count = self->m_history->peak();
    if (max_count == 0) {
        return;
    }

    lv_layer_t * layer = lv_event_get_layer(event);
    lv_obj_t * obj = static_cast<lv_obj_t *>(lv_event_get_target(event));

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    int chart_height = coords.y2 - coords.y1;

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_black();
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = 0;
    dsc.border_width = 0;

    int sample_count = self->m_history->count();
    for (int i = 0; i < sample_count; i++) {
        const HistorySample & sample = self->m_history->at(i);
        if (sample.count <= 0) {
            continue;
        }
        int bar_height = (int)((int64_t)sample.count * chart_height / max_count);
        if (bar_height < 1) {
            bar_height = 1;
        }
        lv_area_t bar_area;
        bar_area.x1 = (lv_coord_t)(coords.x1 + i * 6);
        bar_area.x2 = (lv_coord_t)(coords.x1 + i * 6 + 4);
        bar_area.y1 = (lv_coord_t)(coords.y2 - bar_height);
        bar_area.y2 = coords.y2;
        lv_draw_rect(layer, &dsc, &bar_area);
    }
}
