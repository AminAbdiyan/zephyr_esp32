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
├── app.overlay                       # Optional global Devicetree overlay
├── boards/                           # Board-specific configurations and overlays
│   ├── <board_name>.conf             # Board-specific Kconfig additions
│   └── <board_name>.overlay          # Board-specific Devicetree overrides
├── include/
│   └── app/
│       ├── app_status.hpp            # Shared status/error type
│       └── <module>/<module>.hpp     # Public module interface
├── src/
│   ├── main.cpp                      # Application startup and dependency wiring
│   ├── platform/                     # Zephyr and hardware adapters
│   └── <module>/<module>.cpp         # Module implementation
├── tests/                            # Zephyr ztest unit/integration tests
├── docs/                             # Architecture or safety notes, if needed
├── .clang-format
└── .clang-tidy