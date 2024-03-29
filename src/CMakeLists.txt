cmake_minimum_required(VERSION 3.12)

add_executable(main
        main.c
        pico_tasks.c
        drivers/uart/uart_driver.c
        drivers/tcp/tcp_driver.c
        )

set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "WiFi SSID")
set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "WiFi Password")
set(CONTROLLER_IP "${CONTROLLER_IP}" CACHE INTERNAL "Controller IP")
set(DEVICE_ID "${DEVICE_ID}" CACHE INTERNAL "Device ID")

target_compile_definitions(main PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        CONTROLLER_IP=\"${CONTROLLER_IP}\"
        DEVICE_ID=\"${DEVICE_ID}\"
        )

target_include_directories(main PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        )

target_link_libraries(main 
        pico_stdlib 
        pico_cyw43_arch_lwip_poll
        FreeRTOS
        )

pico_enable_stdio_usb(main 1)

pico_add_extra_outputs(main)
