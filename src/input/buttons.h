#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

enum class ButtonId {
    HOME = 0,
    EXIT = 1,
    UP = 2,
    DOWN = 3,
    OK = 4,
};

struct ButtonEvent {
    ButtonId id;
};

class Buttons {
public:
    Buttons() = default;
    ~Buttons() = default;

    Buttons(const Buttons &) = delete;
    Buttons & operator=(const Buttons &) = delete;

    void init(QueueHandle_t event_queue);

    // Start the timer that re-enables button interrupts after release; call
    // after receiving a button event (level ISRs disable themselves on fire)
    void rearm();

private:
    QueueHandle_t m_event_queue = nullptr;
};
