cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(attitude_control C CXX ASM)
pico_sdk_init()
add_executable(attitude_control attitude_control.cpp)
target_link_libraries(attitude_control pico_stdlib pico_cyw43_arch_none hardware_i2c hardware_pwm)
# USB serial for stdio (printf), not the default UART - which would otherwise
# use GPIO0/GPIO1, the same pins wired to the MPU6050's I2C bus.
pico_enable_stdio_usb(attitude_control 1)
pico_enable_stdio_uart(attitude_control 0)
pico_add_extra_outputs(attitude_control)
