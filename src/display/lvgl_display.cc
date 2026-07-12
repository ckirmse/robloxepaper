#include "display/lvgl_display.h"

#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "log.h"

static const char * TAG = "LVD";

// RGB565: white = 0xFFFF, black = 0x0000. Threshold at midpoint.
static constexpr uint16_t WHITE_THRESHOLD = 0x8000;
static constexpr int EPD_ROW_BYTES = EPD_WIDTH / 8;

void lvglFlushCallback(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    LvglDisplay * self = static_cast<LvglDisplay *>(lv_display_get_user_data(disp));

    // Convert RGB565 → 1-bit and accumulate into epaper buffer
    const uint16_t * src = reinterpret_cast<const uint16_t *>(px_map);
    int area_w = area->x2 - area->x1 + 1;

    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint16_t pixel = src[(y - area->y1) * area_w + (x - area->x1)];
            bool white = (pixel >= WHITE_THRESHOLD);
            int byte_idx = y * EPD_ROW_BYTES + x / 8;
            int bit_idx = 7 - (x % 8);  // MSB = leftmost pixel
            if (white) {
                self->m_epaper_buf[byte_idx] |= (uint8_t)(1 << bit_idx);
            } else {
                self->m_epaper_buf[byte_idx] &= (uint8_t)~(1 << bit_idx);
            }
        }
    }

    // Accumulate the union of all dirty regions in this tick
    if (!self->m_has_dirty) {
        self->m_dirty_area = *area;
        self->m_has_dirty = true;
    } else {
        if (area->x1 < self->m_dirty_area.x1) {
            self->m_dirty_area.x1 = area->x1;
        }
        if (area->y1 < self->m_dirty_area.y1) {
            self->m_dirty_area.y1 = area->y1;
        }
        if (area->x2 > self->m_dirty_area.x2) {
            self->m_dirty_area.x2 = area->x2;
        }
        if (area->y2 > self->m_dirty_area.y2) {
            self->m_dirty_area.y2 = area->y2;
        }
    }

    if (lv_display_flush_is_last(disp)) {
        if (self->m_first_refresh) {
            self->m_first_refresh = false;
            self->m_epaper.fullRefresh(self->m_epaper_buf);
        } else {
            self->m_epaper.fastRefreshPartial(self->m_epaper_buf,
                self->m_dirty_area.x1, self->m_dirty_area.y1,
                self->m_dirty_area.x2, self->m_dirty_area.y2);
        }
        self->m_has_dirty = false;
    }

    lv_display_flush_ready(disp);
}

LvglDisplay::LvglDisplay(Epaper & epaper) :
    m_epaper(epaper) {}

LvglDisplay::~LvglDisplay() {
    heap_caps_free(m_lv_buf);
    free(m_epaper_buf);
}

void LvglDisplay::init() {
    lv_init();

    lv_tick_set_cb([]() -> uint32_t {
        return (uint32_t)(esp_timer_get_time() / 1000ULL);
    });

    // 1-bit epaper conversion buffer (15KB, internal SRAM)
    m_epaper_buf = static_cast<uint8_t *>(malloc(EPD_BUF_SIZE));
    if (!m_epaper_buf) {
        eprintf(TAG, "Failed to allocate epaper buffer");
        return;
    }
    memset(m_epaper_buf, 0xFF, EPD_BUF_SIZE);  // all white

    // RGB565 draw buffer in PSRAM — LVGL renders here, flush callback converts to 1-bit
    const size_t lv_buf_size = EPD_WIDTH * EPD_HEIGHT * sizeof(uint16_t);
    m_lv_buf = static_cast<uint8_t *>(heap_caps_malloc(lv_buf_size, MALLOC_CAP_SPIRAM));
    if (!m_lv_buf) {
        eprintf(TAG, "Failed to allocate LVGL draw buffer in PSRAM");
        return;
    }

    m_lv_display = lv_display_create(EPD_WIDTH, EPD_HEIGHT);
    lv_display_set_user_data(m_lv_display, this);
    lv_display_set_flush_cb(m_lv_display, lvglFlushCallback);
    lv_display_set_buffers(m_lv_display, m_lv_buf, nullptr, lv_buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lprintf(TAG, "LVGL initialized");
}

uint32_t LvglDisplay::tick() {
    uint32_t next_ms = lv_timer_handler();
    if (next_ms < 10) {
        next_ms = 10;
    }
    if (next_ms > 1000) {
        next_ms = 1000;
    }
    return next_ms;
}

void LvglDisplay::requestFullRefresh() {
    m_first_refresh = true;
}
