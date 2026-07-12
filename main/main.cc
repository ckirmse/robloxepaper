#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "app.h"

static const char * TAG = "MAIN";

static void run_task(void *) {
    App app;
    app.init();
    while (true) {
        app.poll();
    }
}

extern "C" void app_main() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Starting epaper app");
    xTaskCreatePinnedToCore(run_task, "app_run", 16 * 1024, nullptr, 1, nullptr, 1);
}
