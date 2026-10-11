cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Requires PICO_SDK_PATH to be set, and pico_sdk_import.cmake copied
# from $PICO_SDK_PATH/external/ into this folder.
include(pico_sdk_import.cmake)

project(tracker_bringup C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(tracker_bringup tracker_bringup.cpp)
target_link_libraries(tracker_bringup pico_stdlib hardware_pwm hardware_clocks)

pico_enable_stdio_usb(tracker_bringup 1)
pico_enable_stdio_uart(tracker_bringup 0)

pico_add_extra_outputs(tracker_bringup)