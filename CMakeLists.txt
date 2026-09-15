# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/overlay/esp32_devkitc_procpu.overlay" CACHE STRING "" FORCE)

# Ensure ZEPHYR_BASE is exported to subprocess environments (needed for west commands)
if(NOT DEFINED ENV{ZEPHYR_BASE})
    set(ENV{ZEPHYR_BASE} "C:/Users/amin_/Desktop/Projekte/zephyr/zephyr_west_sdk/esp32/deps/zephyr")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_esp32_lcd)

target_sources(app PRIVATE
    src/main.cpp
    src/display/display_task.cpp
    src/http/http_server_task.cpp
    src/platform/zephyr_auxdisplay_adapter.cpp
    src/platform/zephyr_mpu6050_adapter.cpp
    src/sensor/sensor_task.cpp
    src/storage/credentials_storage.cpp
    src/terminal/terminal_input.cpp
    src/wifi/wifi_manager.cpp
    src/wifi/wifi_orchestrator.cpp
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
