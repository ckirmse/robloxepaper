#pragma once

#include <stdint.h>
#include <stddef.h>

static constexpr int EPD_WIDTH = 400;
static constexpr int EPD_HEIGHT = 300;
static constexpr int EPD_BUF_SIZE = EPD_WIDTH * EPD_HEIGHT / 8;  // 15000 bytes

static constexpr int EPD_PIN_SCK = 12;
static constexpr int EPD_PIN_MOSI = 11;
static constexpr int EPD_PIN_RST = 47;
static constexpr int EPD_PIN_DC = 46;
static constexpr int EPD_PIN_CS = 45;
static constexpr int EPD_PIN_BUSY = 48;
static constexpr int EPD_PIN_PWR_DISPLAY = 7;
static constexpr int EPD_PIN_PWR_SYSTEM = 41;

class Epaper {
public:
    Epaper() = default;
    ~Epaper() = default;

    Epaper(const Epaper &) = delete;
    Epaper & operator=(const Epaper &) = delete;

    void init();

    // Full refresh — call on first use after power-on to eliminate ghosting (~4s)
    void fullRefresh(const uint8_t * buf);

    // Fast refresh (~1s) — use for routine updates after at least one fullRefresh
    void fastRefresh(const uint8_t * buf);

    void sleep();

private:
    bool m_initialized = false;
    uint8_t * m_prev_frame = nullptr;

    void powerOn();
    void gpioInit();
    void reset();
    void waitBusy();
    void writeByte(uint8_t byte);
    void writeCmd(uint8_t cmd);
    void writeData(uint8_t dat);
    void setAddress(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);
    void setCursor(uint16_t xs, uint16_t ys);
    void initFull();
    void initFast();
    void triggerFullUpdate();
    void triggerFastUpdate();
    void writeBuffer(const uint8_t * buf);
    void writePrevBuffer(const uint8_t * buf);
};
