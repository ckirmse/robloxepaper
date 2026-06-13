#include "display/lvgl_display.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "log.h"

static const char * TAG = "LVD";

// LVGL RGB565: white = 0xFFFF, black = 0x0000.
// Threshold at mid-range: >= 0x8000 is white.
static constexpr uint16_t WHITE_THRESHOLD = 0x8000;

// Bytes per row in the 1-bit e-paper buffer
static constexpr int EPD_ROW_BYTES = EPD_WIDTH / 8;

void lvglFlushCallback(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    LvglDisplay * self = static_cast<LvglDisplay *>(lv_display_get_user_data(disp));

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

    if (lv_display_flush_is_last(disp)) {
        if (self->m_first_refresh) {
            self->m_first_refresh = false;
            self->m_epaper.fullRefresh(self->m_epaper_buf);
        } else {
            self->m_epaper.fastRefresh(self->m_epaper_buf);
        }
    }

    lv_display_flush_ready(disp);
}

LvglDisplay::LvglDisplay(Epaper & epaper)
    : m_epaper(epaper)
    , m_lv_display(nullptr)
    , m_lv_buf(nullptr)
    , m_epaper_buf(nullptr)
    , m_first_refresh(true) {}

LvglDisplay::~LvglDisplay() {
    if (m_lv_buf) {
        heap_caps_free(m_lv_buf);
    }
    if (m_epaper_buf) {
        free(m_epaper_buf);
    }
}

void LvglDisplay::init() {
    lv_init();

    // Use esp_timer for LVGL's tick source (microseconds → milliseconds)
    lv_tick_set_cb([]() -> uint32_t {
        return (uint32_t)(esp_timer_get_time() / 1000ULL);
    });

    // Allocate 1-bit e-paper conversion buffer (15KB, internal SRAM)
    m_epaper_buf = static_cast<uint8_t *>(malloc(EPD_BUF_SIZE));
    if (!m_epaper_buf) {
        eprintf(TAG, "Failed to allocate e-paper buffer");
        return;
    }
    memset(m_epaper_buf, 0xFF, EPD_BUF_SIZE);  // all white

    // Allocate draw buffer in PSRAM: full RGB565 framebuffer (240KB)
    const size_t buf_size = EPD_WIDTH * EPD_HEIGHT * sizeof(uint16_t);
    m_lv_buf = static_cast<uint8_t *>(heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM));
    if (!m_lv_buf) {
        eprintf(TAG, "Failed to allocate LVGL draw buffer in PSRAM");
        return;
    }

    m_lv_display = lv_display_create(EPD_WIDTH, EPD_HEIGHT);
    lv_display_set_user_data(m_lv_display, this);
    lv_display_set_flush_cb(m_lv_display, lvglFlushCallback);
    lv_display_set_buffers(m_lv_display, m_lv_buf, nullptr, buf_size,
                           LV_DISPLAY_RENDER_MODE_FULL);

    lprintf(TAG, "LVGL initialized, draw buffer %u bytes in PSRAM", (unsigned)buf_size);
}

void LvglDisplay::tick() {
    lv_timer_handler();
}

void LvglDisplay::requestFullRefresh() {
    m_first_refresh = true;
}
