#include "display/epaper.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

#include "log.h"

static const char * TAG = "EPD";

static inline void setPin(int pin, int level) {
    gpio_set_level((gpio_num_t)pin, level);
}

static inline int readPin(int pin) {
    return gpio_get_level((gpio_num_t)pin);
}


void Epaper::powerOn() {
    setPin(EPD_PIN_PWR_SYSTEM, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    setPin(EPD_PIN_PWR_DISPLAY, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Epaper::gpioInit() {
    gpio_config_t out_cfg = {};
    out_cfg.mode = GPIO_MODE_OUTPUT;
    out_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    out_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out_cfg.intr_type = GPIO_INTR_DISABLE;
    out_cfg.pin_bit_mask =
        (1ULL << EPD_PIN_SCK) |
        (1ULL << EPD_PIN_MOSI) |
        (1ULL << EPD_PIN_RST) |
        (1ULL << EPD_PIN_DC) |
        (1ULL << EPD_PIN_CS) |
        (1ULL << EPD_PIN_PWR_DISPLAY) |
        (1ULL << EPD_PIN_PWR_SYSTEM);
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {};
    in_cfg.mode = GPIO_MODE_INPUT;
    in_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    in_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    in_cfg.intr_type = GPIO_INTR_DISABLE;
    in_cfg.pin_bit_mask = (1ULL << EPD_PIN_BUSY);
    gpio_config(&in_cfg);

    setPin(EPD_PIN_CS, 1);
    setPin(EPD_PIN_DC, 1);
}

void Epaper::init() {
    gpioInit();
    powerOn();
    m_prev_frame = static_cast<uint8_t *>(heap_caps_malloc(EPD_BUF_SIZE, MALLOC_CAP_SPIRAM));
    m_initialized = true;
    lprintf(TAG, "GPIO initialized, power on");
}

void Epaper::reset() {
    setPin(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    setPin(EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    setPin(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Epaper::waitBusy() {
    // BUSY HIGH = chip is processing; BUSY LOW = chip is ready
    while (readPin(EPD_PIN_BUSY) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Epaper::writeByte(uint8_t byte) {
    setPin(EPD_PIN_CS, 0);
    for (int i = 0; i < 8; i++) {
        setPin(EPD_PIN_SCK, 0);
        setPin(EPD_PIN_MOSI, (byte & 0x80) ? 1 : 0);
        setPin(EPD_PIN_SCK, 1);
        byte <<= 1;
    }
    setPin(EPD_PIN_CS, 1);
}

void Epaper::writeCmd(uint8_t cmd) {
    setPin(EPD_PIN_DC, 0);
    writeByte(cmd);
    setPin(EPD_PIN_DC, 1);
}

void Epaper::writeData(uint8_t dat) {
    setPin(EPD_PIN_DC, 1);
    writeByte(dat);
}

void Epaper::setAddress(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye) {
    writeCmd(0x44);
    writeData((xs >> 3) & 0xFF);
    writeData((xe >> 3) & 0xFF);

    writeCmd(0x45);
    writeData(ys & 0xFF);
    writeData((ys >> 8) & 0xFF);
    writeData(ye & 0xFF);
    writeData((ye >> 8) & 0xFF);
}

void Epaper::setCursor(uint16_t xs, uint16_t ys) {
    writeCmd(0x4E);
    writeData(xs & 0xFF);

    writeCmd(0x4F);
    writeData(ys & 0xFF);
    writeData((ys >> 8) & 0xFF);
}

void Epaper::initFull() {
    reset();
    waitBusy();
    writeCmd(0x12);   // soft reset
    waitBusy();
    writeCmd(0x21);   // display update control
    writeData(0x40);
    writeData(0x00);
    writeCmd(0x3C);   // border waveform
    writeData(0x05);
    writeCmd(0x11);   // data entry mode: X-increment
    writeData(0x03);
    setAddress(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);
    setCursor(0, 0);
}

void Epaper::initFast() {
    reset();
    waitBusy();
    writeCmd(0x12);   // soft reset
    waitBusy();
    writeCmd(0x21);
    writeData(0x40);
    writeData(0x00);
    writeCmd(0x3C);
    writeData(0x05);
    writeCmd(0x1A);   // write to temperature register (1s fast mode)
    writeData(0x5A);
    writeCmd(0x22);   // load temperature value
    writeData(0x91);
    writeCmd(0x20);
    waitBusy();
    writeCmd(0x11);   // data entry mode: X-increment
    writeData(0x03);
    setAddress(0, 0, EPD_WIDTH - 1, EPD_HEIGHT - 1);
    setCursor(0, 0);
}

void Epaper::triggerFullUpdate() {
    writeCmd(0x22);
    writeData(0xF7);
    writeCmd(0x20);
    waitBusy();
}

void Epaper::triggerFastUpdate() {
    writeCmd(0x22);
    writeData(0xC7);
    writeCmd(0x20);
    waitBusy();
}

void Epaper::writeBuffer(const uint8_t * buf) {
    writeCmd(0x24);
    for (int i = 0; i < EPD_BUF_SIZE; i++) {
        writeData(buf[i]);
    }
}

void Epaper::writePrevBuffer(const uint8_t * buf) {
    writeCmd(0x26);
    for (int i = 0; i < EPD_BUF_SIZE; i++) {
        writeData(buf[i]);
    }
}

void Epaper::fullRefresh(const uint8_t * buf) {
    lprintf(TAG, "Full refresh starting");
    initFull();
    writeBuffer(buf);
    triggerFullUpdate();
    memcpy(m_prev_frame, buf, EPD_BUF_SIZE);
    sleep();
    lprintf(TAG, "Full refresh done");
}

void Epaper::fastRefresh(const uint8_t * buf) {
    lprintf(TAG, "Fast refresh starting");
    initFast();
    writePrevBuffer(m_prev_frame);
    setCursor(0, 0);
    writeBuffer(buf);
    triggerFastUpdate();
    memcpy(m_prev_frame, buf, EPD_BUF_SIZE);
    sleep();
    lprintf(TAG, "Fast refresh done");
}

void Epaper::fastRefreshPartial(const uint8_t * buf,
                                 int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    // Full buffer write is required: sleep mode 2 does not retain RAM, so after
    // initFast()'s reset the unwritten regions would contain garbage. Fast refresh
    // naturally only flickers pixels where old != new, so visually only the dirty
    // region updates.
    lprintf(TAG, "Fast partial refresh [%" PRId32 ",%" PRId32 "]-[%" PRId32 ",%" PRId32 "]",
            x1, y1, x2, y2);
    initFast();
    writePrevBuffer(m_prev_frame);
    setCursor(0, 0);
    writeBuffer(buf);
    triggerFastUpdate();
    memcpy(m_prev_frame, buf, EPD_BUF_SIZE);
    sleep();
    lprintf(TAG, "Fast partial refresh done");
}

void Epaper::sleep() {
    writeCmd(0x10);
    writeData(0x03);  // sleep mode 2: no RAM retention, <1µA
    vTaskDelay(pdMS_TO_TICKS(50));
}
