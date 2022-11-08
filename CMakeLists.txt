cmake_minimum_required(VERSION 3.12)

set(PICO_BOARD pico_w)

include(pico_sdk_import.cmake)

project(ESET-2022)

pico_sdk_init()

add_subdirectory(FreeRTOS)
add_subdirectory(src)