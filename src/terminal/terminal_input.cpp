/**
 * @file terminal_input.cpp
 * @brief Implementation of interactive serial console input using console_getchar().
 */

#include "terminal/terminal_input.hpp"

#include <zephyr/console/console.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <ctype.h>

namespace app::terminal {

Status TerminalInput::Initialize() noexcept
{
    const int ret = console_init();
    if (ret != 0) {
        return Status::kNotReady;
    }
    return Status::kOk;
}

Status TerminalInput::ReadLine(char* const buffer, const size_t max_length, const bool mask_input) noexcept
{
    if ((buffer == nullptr) || (max_length <= 1U)) {
        return Status::kInvalidArgument;
    }

    size_t index = 0U;

    while (true) {
        // Read single ASCII byte from Zephyr console (blocks until character received)
        const int c = console_getchar();
        if (c < 0) {
            // Console read error
            continue;
        }

        // Handle ANSI Escape Sequences (0x1B), e.g. arrow keys, page up/down, delete keys
        if (c == 0x1B) {
            // Check if trailing escape characters are present in UART FIFO within 10ms
            console_set_rx_timeout(K_MSEC(10));
            int esc_byte = 0;
            do {
                esc_byte = console_getchar();
            } while (esc_byte >= 0);
            // Restore blocking mode for subsequent keystrokes
            console_set_rx_timeout(K_FOREVER);
            continue;
        }

        // Handle Carriage Return ('\r') or Newline ('\n')
        if ((c == '\r') || (c == '\n')) {
            // Null-terminate the entered buffer immediately
            buffer[index] = '\0';
            // Echo newline to terminal so the prompt moves cleanly to the next line
            printk("\r\n");

            // Drain paired CRLF or LFCR characters that arrive together in serial packets
            // so they do not leak into and prematurely complete the subsequent input prompt
            console_set_rx_timeout(K_NO_WAIT);
            const int next_c = console_getchar();
            if ((next_c >= 0) && (((c == '\r') && (next_c == '\n')) || ((c == '\n') && (next_c == '\r')))) {
                // Paired line terminator discarded cleanly
            }
            // Restore blocking console reception
            console_set_rx_timeout(K_FOREVER);

            return Status::kOk;
        }

        // Handle Backspace (ASCII 0x08) or Delete (ASCII 0x7F)
        if ((c == '\b') || (c == 0x7F)) {
            if (index > 0U) {
                index--;
                // Visual erase in VT100 / standard serial terminals: backspace, space, backspace
                printk("\b \b");
            }
            continue;
        }

        // Handle printable characters
        if (isprint(c) != 0) {
            // Check if buffer capacity allows this character (leaving room for null terminator)
            if (index < (max_length - 1U)) {
                buffer[index] = static_cast<char>(c);
                index++;

                // Echo back to user
                if (mask_input) {
                    printk("*");
                } else {
                    printk("%c", c);
                }
            }
        }
    }
}

Status TerminalInput::Prompt(
    const char* const prompt_text,
    char* const buffer,
    const size_t max_length,
    const bool mask_input) noexcept
{
    if (prompt_text != nullptr) {
        printk("%s", prompt_text);
    }
    return ReadLine(buffer, max_length, mask_input);
}

}  // namespace app::terminal

