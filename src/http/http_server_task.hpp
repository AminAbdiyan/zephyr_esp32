/**
 * @file http_server_task.hpp
 * @brief Embedded HTTP web server running on port 80 to serve real-time sensor telemetry.
 *
 * Architecture & Safety:
 * - Runs as a dedicated background Zephyr RTOS thread.
 * - Listens on TCP port 80 (HTTP standard).
 * - Serves two endpoints:
 *   1. `GET /`: Clean, responsive HTML dashboard styled with embedded CSS and modern typography,
 *      featuring dynamic JavaScript polling every 1 second.
 *   2. `GET /api/data`: Returns JSON telemetry payload:
 *      `{"accel":{"x":0.0,"y":0.0,"z":9.8},"gyro":{"x":0.0,"y":0.0,"z":0.0},"temp":25.4}`
 * - Reads sensor values non-blockingly from `SensorDataHub`.
 * - Fixed statically allocated thread stack and socket buffers (no dynamic heap allocations).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "common/app_status.hpp"
#include "sensor/sensor_data.hpp"

namespace app::http {

/**
 * @brief Embedded HTTP server task serving HTML/JSON dashboard.
 */
class HttpServerTask final {
public:
    /// Thread stack size in bytes allocated statically in internal SRAM.
    static constexpr size_t kStackSizeBytes = 4096U;

    /// Zephyr preemptible thread priority. Priority 8 runs just below UI and Sensor polling.
    static constexpr int kThreadPriority = 8;

    /// TCP port to bind HTTP server (Standard HTTP: 80).
    static constexpr uint16_t kPort = 80U;

    /**
     * @brief Constructs HTTP server task with injected sensor data hub.
     * @param data_hub Reference to thread-safe sensor repository.
     */
    explicit HttpServerTask(const sensor::SensorDataHub& data_hub) noexcept;
    ~HttpServerTask() = default;

    // Disallow copying and moving
    HttpServerTask(const HttpServerTask&) = delete;
    HttpServerTask& operator=(const HttpServerTask&) = delete;
    HttpServerTask(HttpServerTask&&) = delete;
    HttpServerTask& operator=(HttpServerTask&&) = delete;

    /**
     * @brief Launches the HTTP server thread.
     *
     * @return Status::kOk if thread spawned successfully.
     * @return Status::kBusy if task was already started.
     */
    [[nodiscard]] Status Start() noexcept;

private:
    /**
     * @brief Trampoline entry point for Zephyr `k_thread_create`.
     */
    static void ThreadEntry(void* p1, void* p2, void* p3) noexcept;

    /**
     * @brief Main listener loop accepting incoming TCP client connections.
     */
    void Run() noexcept;

    /**
     * @brief Handles a single incoming HTTP request on an accepted client socket.
     * @param client_sock Connected client socket descriptor.
     */
    void HandleClient(int client_sock) noexcept;

    /// Injected thread-safe sensor data hub.
    const sensor::SensorDataHub& data_hub_;

    /// Prevents duplicate thread start.
    bool is_started_{false};
};

}  // namespace app::http

