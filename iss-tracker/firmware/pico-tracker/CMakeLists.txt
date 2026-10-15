cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Requires PICO_SDK_PATH to be set, and pico_sdk_import.cmake copied
# from $PICO_SDK_PATH/external/ into this folder.
include(pico_sdk_import.cmake)

project(pico_tracker C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_tracker main.cpp tracker_core.cpp)
target_link_libraries(pico_tracker pico_stdlib hardware_pwm hardware_clocks hardware_sync)

pico_enable_stdio_usb(pico_tracker 1)
pico_enable_stdio_uart(pico_tracker 0)

pico_add_extra_outputs(pico_tracker)
