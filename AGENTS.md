# Zephyr Embedded Project — Agent Guide

This file contains the coding, architecture, and documentation rules for this Zephyr application.
Keep the project simple, safe, testable, and easy to understand.

## Project goals

- Build reliable embedded software with Zephyr RTOS.
- Follow safety-oriented programming practices inspired by MISRA C:2012 and MISRA C++ principles.
- Prefer clear and small code over clever or overly generic code.
- Make focused changes. Do not refactor unrelated code while implementing a feature or bug fix.

## Code Documentation & Commenting Standards

Every file and function in this project must be thoroughly commented so that anyone reading the code can immediately understand both *what* it does and *why* it was designed that way:

1. **NEVER Remove or Shorten Existing Comments**:
   - When refactoring, modifying, or adding features, preserve all existing docstrings, rationale, and inline comments.
   - Do not simplify or omit comments to save space. Every change must maintain or increase documentation quality.

2. **Explain the "Why", Not Just the "What"**:
   - Always explain hardware-specific decisions, timing delays, bit-level shifts, and register quirks (citing datasheet requirements where applicable).
   - Document safety and architectural rationales (e.g., why static allocation is preferred over `malloc`, why `noexcept` is applied, or why `enum class Status : int32_t` is used).

3. **Doxygen-Style API Headers**:
   - Use standard Doxygen blocks (`@brief`, `@param`, `@return`) for all public and private class methods, struct definitions, and interface methods.
   - Mark function return error contracts clearly.

4. **Detailed Parameter Annotations for RTOS / Hardware APIs**:
   - For multi-parameter RTOS and hardware calls (e.g. `k_thread_create`, `sensor_channel_get`), annotate every single argument with an inline comment explaining its purpose, pointer type, or units.

5. **Inline Hardware Explanations**:
   - When interacting with buses (I2C, SPI, GPIO) or peripheral expanders (like PCF8574, MPU6050), include diagrams or inline bit-mapping comments explaining how bytes map to physical pins.

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
```

## Codebase Knowledge Graph (codebase-memory-mcp)

When exploring repo architecture, function call chains, or blast radius:
- Use `search_graph` and `query_graph` instead of raw grepping.
- Trace callers and dependencies with `trace_path`.
- Check git blast radius with `detect_changes`.