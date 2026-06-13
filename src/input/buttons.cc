#include "input/buttons.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "log.h"

static const char * TAG = "BTN";

static constexpr int64_t DEBOUNCE_US = 50000;  // 50ms

static constexpr int BUTTON_COUNT = 5;

static const gpio_num_t BUTTON_GPIOS[BUTTON_COUNT] = {
    GPIO_NUM_2,  // HOME
    GPIO_NUM_1,  // EXIT
    GPIO_NUM_6,  // UP
    GPIO_NUM_4,  // DOWN
    GPIO_NUM_5,  // OK
};

static const ButtonId BUTTON_IDS[BUTTON_COUNT] = {
    ButtonId::HOME,
    ButtonId::EXIT,
    ButtonId::UP,
    ButtonId::DOWN,
    ButtonId::OK,
};

struct ButtonIsrArg {
    QueueHandle_t queue;
    ButtonId id;
    gpio_num_t gpio;
    int64_t last_fire_us;
};

static ButtonIsrArg s_isr_args[BUTTON_COUNT];

static void IRAM_ATTR buttonIsrHandler(void * arg) {
    ButtonIsrArg * btn = static_cast<ButtonIsrArg *>(arg);

    int64_t now = esp_timer_get_time();
    if (now - btn->last_fire_us < DEBOUNCE_US) {
        return;
    }
    btn->last_fire_us = now;

    // Confirm press (active-low: GPIO should be LOW)
    if (gpio_get_level(btn->gpio) != 0) {
        return;
    }

    ButtonEvent event = {btn->id};
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(btn->queue, &event, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}


void Buttons::init(QueueHandle_t event_queue) {
    m_event_queue = event_queue;

    gpio_install_isr_service(0);

    gpio_config_t cfg = {};
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_NEGEDGE;

    for (int i = 0; i < BUTTON_COUNT; i++) {
        cfg.pin_bit_mask = (1ULL << BUTTON_GPIOS[i]);
        gpio_config(&cfg);

        s_isr_args[i].queue = m_event_queue;
        s_isr_args[i].id = BUTTON_IDS[i];
        s_isr_args[i].gpio = BUTTON_GPIOS[i];
        s_isr_args[i].last_fire_us = 0;

        gpio_isr_handler_add(BUTTON_GPIOS[i], buttonIsrHandler, &s_isr_args[i]);
    }

    lprintf(TAG, "Buttons initialized (%d GPIOs)", BUTTON_COUNT);
}
