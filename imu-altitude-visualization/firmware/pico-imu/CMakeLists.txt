cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico2_w)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(imu_reader C CXX ASM)
pico_sdk_init()
add_executable(imu_reader imu_reader.cpp)
target_link_libraries(imu_reader pico_stdlib pico_cyw43_arch_none hardware_i2c)
# USB serial for stdio (printf), not the default UART - which would otherwise
# use GPIO0/GPIO1, the same pins wired to the MPU6050's I2C bus.
pico_enable_stdio_usb(imu_reader 1)
pico_enable_stdio_uart(imu_reader 0)
pico_add_extra_outputs(imu_reader)
