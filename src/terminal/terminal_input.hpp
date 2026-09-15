/**
 * @file terminal_input.hpp
 * @brief Interactive character-by-character serial console input handler.
 *
 * Architecture & Safety:
 * - Uses Zephyr's `console_getchar()` API (`CONFIG_CONSOLE_GETCHAR=y`).
 * - Provides non-blocking/blocking line input with character echoing, backspace handling,
 *   and newline termination detection (`\r` and `\n`).
 * - For password entry, supports masking with asterisks (`*`) or hiding characters to prevent
 *   shoulder-surfing over serial logs.
 * - Statically bounded buffer prevents buffer overruns.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"

namespace app::terminal {

/**
 * @brief Interactive terminal reader for serial console input.
 */
class TerminalInput final {
public:
    TerminalInput() noexcept = default;
    ~TerminalInput() = default;

    // Disallow copy/move
    TerminalInput(const TerminalInput&) = delete;
    TerminalInput& operator=(const TerminalInput&) = delete;
    TerminalInput(TerminalInput&&) = delete;
    TerminalInput& operator=(TerminalInput&&) = delete;

    /**
     * @brief Initializes the console subsystem driver for character input.
     *
     * @return Status::kOk on success, or Status::kNotReady on failure.
     */
    [[nodiscard]] Status Initialize() noexcept;

    /**
     * @brief Reads a single line of text from the serial console.
     *
     * Handles:
     * - Enter key (`\r` or `\n`): ends input and null-terminates the buffer.
     * - Backspace (`\b` or ASCII 127): erases the last character from buffer and terminal.
     * - Masking: if `mask_input` is true, prints `*` instead of the typed character.
     *
     * @param[out] buffer Destination buffer for the entered string.
     * @param[in]  max_length Maximum size of the buffer including null terminator.
     * @param[in]  mask_input If true, echoes '*' instead of the plaintext character (for passwords).
     * @return Status::kOk when a line has been successfully entered.
     * @return Status::kInvalidArgument if buffer is null or max_length is zero.
     */
    [[nodiscard]] Status ReadLine(char* buffer, size_t max_length, bool mask_input = false) noexcept;

    /**
     * @brief Prompts the user and reads a line of input.
     *
     * @param[in]  prompt_text Prompt string to print before reading input.
     * @param[out] buffer Destination buffer.
     * @param[in]  max_length Maximum size of the buffer.
     * @param[in]  mask_input If true, mask input characters with '*'.
     * @return Status::kOk on success.
     */
    [[nodiscard]] Status Prompt(
        const char* prompt_text,
        char* buffer,
        size_t max_length,
        bool mask_input = false) noexcept;
};

}  // namespace app::terminal

