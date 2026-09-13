# Zephyr Embedded Project — Agent Guide

This file contains the coding and workflow rules for this Zephyr application.
Keep the project simple, safe, testable, and easy to understand.

## Project goals

- Build reliable embedded software with Zephyr RTOS.
- Follow safety-oriented programming practices inspired by MISRA C:2012 and MISRA C++ principles.
- Prefer clear and small code over clever or overly generic code.
- Make focused changes. Do not refactor unrelated code while implementing a feature or bug fix.

## Project structure

Use the following structure as the project grows:

```text
.
├── AGENTS.md
├── README.md                         # Setup, build, usage, and important changes
├── CMakeLists.txt
├── prj.conf                          # Base Zephyr application configuration
├── overlay/                          # Devicetree overlays
│   └── <board_name>.overlay          # Board-specific Devicetree overlay
├── src/
│   ├── main.cpp                      # Application startup and dependency wiring
│   ├── common/                       # Shared types, error codes, and utilities
│   │   └── app_status.hpp
│   ├── platform/                     # Zephyr and hardware adapters (.hpp and .cpp)
│   │   ├── <adapter>.hpp
│   │   └── <adapter>.cpp
│   └── <module>/                     # Module interface and implementation (.hpp and .cpp)
│       ├── <module>_service.hpp
│       ├── <module>_task.hpp
│       └── <module>_task.cpp
├── tests/                            # Zephyr ztest unit/integration tests
├── docs/                             # Architecture or safety notes, if needed
├── .clang-format
└── .clang-tidy