#pragma once

#include <stdint.h>

#include "lvgl.h"

#include "display/epaper.h"

class LvglDisplay {
public:
    explicit LvglDisplay(Epaper & epaper);
    ~LvglDisplay();

    LvglDisplay(const LvglDisplay &) = delete;
    LvglDisplay & operator=(const LvglDisplay &) = delete;

    void init();

    // Call from main loop — handles LVGL timers and triggers e-paper refresh when
    // dirty. Returns ms until the next LVGL timer is due, clamped to [10, 1000],
    // so the caller can block that long
    uint32_t tick();

    // Force the next e-paper update to use a full (ghosting-clearing) refresh
    void requestFullRefresh();

private:
    Epaper & m_epaper;
    lv_display_t * m_lv_display = nullptr;
    uint8_t * m_lv_buf = nullptr;   // 240KB RGB565 draw buffer in PSRAM
    uint8_t * m_epaper_buf = nullptr; // 15KB 1-bit epaper buffer, heap-allocated
    bool m_first_refresh = true;
    lv_area_t m_dirty_area = {};
    bool m_has_dirty = false;

    friend void lvglFlushCallback(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
};
