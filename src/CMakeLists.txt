idf_component_register(
    SRCS
        "app.cc"
        "display/epaper.cc"
        "display/lvgl_display.cc"
        "network/wifi.cc"
        "network/http_poller.cc"
        "input/buttons.cc"
        "screens/player_count.cc"
        "fonts/lv_font_press_start_2p_80.c"
        "fonts/lv_font_builder_sans_semibold_20.c"
        "fonts/lv_font_builder_sans_semibold_28.c"
        "fonts/lv_font_builder_sans_semibold_16.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_driver_gpio
        esp_timer
        esp_netif
        esp_wifi
        esp_http_client
        mbedtls
        lvgl__lvgl
        freertos
        esp_system
        esp_hw_support
)

# Font files need "lvgl.h" (not "lvgl/lvgl.h") since the component root is the include dir
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
