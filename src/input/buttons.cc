#include "input/buttons.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_sleep.h"

#include "log.h"

static const char * TAG = "BTN";

static constexpr int64_t DEBOUNCE_US = 50000;  // 50ms

static constexpr int64_t REARM_PERIOD_US = 50000;  // 50ms

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
    volatile bool intr_disabled;
};

static ButtonIsrArg s_isr_args[BUTTON_COUNT];

static esp_timer_handle_t s_rearm_timer = nullptr;

// Buttons use LOW_LEVEL interrupts (required for light-sleep GPIO wakeup), so
// the ISR must disable its own interrupt to avoid storming while the button is
// held. The rearm timer re-enables interrupts once buttons are released.
static void IRAM_ATTR buttonIsrHandler(void * arg) {
    ButtonIsrArg * btn = static_cast<ButtonIsrArg *>(arg);

    gpio_intr_disable(btn->gpio);
    btn->intr_disabled = true;

    // Confirm press (active-low: GPIO should be LOW)
    if (gpio_get_level(btn->gpio) != 0) {
        btn->intr_disabled = false;
        gpio_intr_enable(btn->gpio);
        return;
    }

    int64_t now = esp_timer_get_time();
    if (now - btn->last_fire_us < DEBOUNCE_US) {
        // Bounce of an already-reported press; stay disabled until released
        return;
    }
    btn->last_fire_us = now;

    ButtonEvent event = {btn->id};
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(btn->queue, &event, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void rearmTimerCallback(void *) {
    int64_t now = esp_timer_get_time();
    bool all_idle = true;
    for (int i = 0; i < BUTTON_COUNT; i++) {
        ButtonIsrArg & btn = s_isr_args[i];
        if (btn.intr_disabled) {
            if (gpio_get_level(btn.gpio) == 1) {
                btn.intr_disabled = false;
                gpio_intr_enable(btn.gpio);
            } else {
                all_idle = false;
            }
        }
        // Keep ticking through the debounce window so a bounce that re-disables
        // the interrupt without posting an event still gets rearmed
        if (btn.intr_disabled || now - btn.last_fire_us < DEBOUNCE_US) {
            all_idle = false;
        }
    }
    if (all_idle) {
        esp_timer_stop(s_rearm_timer);
    }
}


void Buttons::init(QueueHandle_t event_queue) {
    m_event_queue = event_queue;

    gpio_install_isr_service(0);

    gpio_config_t cfg = {};
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_LOW_LEVEL;

    for (int i = 0; i < BUTTON_COUNT; i++) {
        cfg.pin_bit_mask = (1ULL << BUTTON_GPIOS[i]);
        gpio_config(&cfg);

        s_isr_args[i].queue = m_event_queue;
        s_isr_args[i].id = BUTTON_IDS[i];
        s_isr_args[i].gpio = BUTTON_GPIOS[i];
        s_isr_args[i].last_fire_us = 0;
        s_isr_args[i].intr_disabled = false;

        gpio_isr_handler_add(BUTTON_GPIOS[i], buttonIsrHandler, &s_isr_args[i]);
        ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON_GPIOS[i], GPIO_INTR_LOW_LEVEL));
    }

    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = rearmTimerCallback;
    timer_args.name = "btn_rearm";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_rearm_timer));

    lprintf(TAG, "Buttons initialized (%d GPIOs, light-sleep wakeup enabled)", BUTTON_COUNT);
}

void Buttons::rearm() {
    if (!esp_timer_is_active(s_rearm_timer)) {
        esp_timer_start_periodic(s_rearm_timer, REARM_PERIOD_US);
    }
}
