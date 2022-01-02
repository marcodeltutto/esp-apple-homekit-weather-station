idf_component_register(SRCS ./u8g2_esp32_hal.c ./app_wifi.c ./app_main.c ./DB.c ./App.c
                       INCLUDE_DIRS ".")
add_definitions(-DHAP_LOG_LEVEL=${CONFIG_HAP_LOG_LEVEL})
