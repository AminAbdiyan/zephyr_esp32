# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/overlay/esp32_devkitc_procpu.overlay" CACHE STRING "" FORCE)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_esp32_lcd)

target_sources(app PRIVATE
    src/main.cpp
    src/display/display_task.cpp
    src/platform/zephyr_auxdisplay_adapter.cpp
    src/platform/zephyr_mpu6050_adapter.cpp
    src/sensor/sensor_task.cpp
)

target_include_directories(app PRIVATE
    src
)

target_compile_options(app PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        -fno-exceptions
        -fno-rtti
        -fno-threadsafe-statics
        -Wall
        -Wextra
    >
)
