#pragma once

#include "esp_log.h"

#define dprintf(fmt, ...) ESP_LOGD("DBG", fmt, ##__VA_ARGS__)
#define lprintf(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define eprintf(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
