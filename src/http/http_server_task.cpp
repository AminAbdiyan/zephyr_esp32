/**
 * @file http_server_task.cpp
 * @brief Implementation of embedded HTTP web server on ESP32 Zephyr RTOS.
 */

#include "http/http_server_task.hpp"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <string.h>

// Register logging module for HTTP server
LOG_MODULE_REGISTER(http_server, LOG_LEVEL_INF);

namespace app::http {

namespace {
// Thread control block and stack statically allocated in BSS
K_THREAD_STACK_DEFINE(g_http_server_stack, HttpServerTask::kStackSizeBytes);
struct k_thread g_http_server_thread {};

/// Clean, modern, responsive single-page web dashboard stored in Flash (RODATA)
const char kHtmlIndex[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32 MPU6050 Live Dashboard</title>"
    "<style>"
    "body{font-family:system-ui,-apple-system,sans-serif;margin:0;padding:20px;background:#0f172a;color:#f8fafc;display:flex;flex-direction:column;align-items:center}"
    "h1{font-size:1.6rem;margin-bottom:6px;color:#38bdf8;text-align:center}"
    ".sub{font-size:0.9rem;color:#94a3b8;margin-bottom:24px;text-align:center}"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px;width:100%;max-width:900px}"
    ".card{background:#1e293b;border-radius:12px;padding:20px;border:1px solid #334155;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3)}"
    ".card h2{margin-top:0;font-size:1.1rem;color:#cbd5e1;border-bottom:1px solid #334155;padding-bottom:8px;display:flex;justify-content:space-between;align-items:center}"
    ".val-row{display:flex;justify-content:space-between;align-items:baseline;margin:12px 0}"
    ".val-lbl{font-size:1rem;color:#94a3b8;font-weight:600}"
    ".val-num{font-size:1.4rem;font-family:monospace;font-weight:bold;color:#f1f5f9}"
    ".badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:0.75rem;background:#0369a1;color:#bae6fd}"
    "#status{margin-top:20px;font-size:0.85rem;color:#64748b}"
    "</style></head><body>"
    "<h1>ESP32 Zephyr MPU6050 Dashboard</h1>"
    "<div class='sub'>Real-time IMU Telemetry (Live 1 Hz updates)</div>"
    "<div class='grid'>"
    "<div class='card'><h2>Acceleration <span class='badge'>m/s&sup2;</span></h2>"
    "<div class='val-row'><span class='val-lbl'>X-Axis:</span><span class='val-num' id='ax'>--</span></div>"
    "<div class='val-row'><span class='val-lbl'>Y-Axis:</span><span class='val-num' id='ay'>--</span></div>"
    "<div class='val-row'><span class='val-lbl'>Z-Axis:</span><span class='val-num' id='az'>--</span></div>"
    "</div>"
    "<div class='card'><h2>Gyroscope <span class='badge'>rad/s</span></h2>"
    "<div class='val-row'><span class='val-lbl'>X-Axis:</span><span class='val-num' id='gx'>--</span></div>"
    "<div class='val-row'><span class='val-lbl'>Y-Axis:</span><span class='val-num' id='gy'>--</span></div>"
    "<div class='val-row'><span class='val-lbl'>Z-Axis:</span><span class='val-num' id='gz'>--</span></div>"
    "</div>"
    "<div class='card'><h2>Die Temperature <span class='badge'>&deg;C</span></h2>"
    "<div class='val-row'><span class='val-lbl'>Temp:</span><span class='val-num' id='tp'>--</span></div>"
    "<div class='val-row'><span class='val-lbl'>Timestamp:</span><span class='val-num' id='ts' style='font-size:1rem'>--</span></div>"
    "</div>"
    "</div>"
    "<div id='status'>Connecting to ESP32 telemetry...</div>"
    "<script>"
    "async function updateData(){"
    " try{"
    "  const res=await fetch('/api/data?t='+Date.now());"
    "  if(res.ok){"
    "   const d=await res.json();"
    "   document.getElementById('ax').innerText=d.accel.x.toFixed(2);"
    "   document.getElementById('ay').innerText=d.accel.y.toFixed(2);"
    "   document.getElementById('az').innerText=d.accel.z.toFixed(2);"
    "   document.getElementById('gx').innerText=d.gyro.x.toFixed(3);"
    "   document.getElementById('gy').innerText=d.gyro.y.toFixed(3);"
    "   document.getElementById('gz').innerText=d.gyro.z.toFixed(3);"
    "   document.getElementById('tp').innerText=d.temp.toFixed(1)+' \u00B0C';"
    "   document.getElementById('ts').innerText=d.timestamp_ms+' ms';"
    "   document.getElementById('status').innerText='Live: updated at '+(new Date()).toLocaleTimeString();"
    "  }else{"
    "   document.getElementById('status').innerText='HTTP error: '+res.status;"
    "  }"
    " }catch(e){"
    "  document.getElementById('status').innerText='Connection error: '+e.message;"
    " }"
    "}"
    "setInterval(updateData,1000);"
    "updateData();"
    "</script></body></html>";

/**
 * @brief Transmits an entire data buffer over a TCP socket, handling partial writes in a loop.
 *
 * Safety & Architecture:
 * - Solves TCP MTU fragmentation where `zsock_send()` returns before all bytes are queued.
 * - Guarantees full delivery of large HTML documents and JSON responses.
 *
 * @param[in] sock Connected TCP client socket.
 * @param[in] data Pointer to contiguous memory buffer.
 * @param[in] total_len Exact number of bytes to transmit.
 * @return True if all bytes were sent successfully, false on socket error.
 */
bool SendAll(const int sock, const void* const data, size_t total_len) noexcept
{
    const char* ptr = static_cast<const char*>(data);
    while (total_len > 0U) {
        const ssize_t sent = zsock_send(sock, ptr, total_len, 0);
        if (sent <= 0) {
            return false;
        }
        ptr += sent;
        total_len -= static_cast<size_t>(sent);
    }
    return true;
}

}  // namespace

HttpServerTask::HttpServerTask(const sensor::SensorDataHub& data_hub) noexcept
    : data_hub_(data_hub)
{
}

Status HttpServerTask::Start() noexcept
{
    if (is_started_) {
        return Status::kBusy;
    }

    const k_tid_t tid = k_thread_create(
        &g_http_server_thread,                       // Thread control block
        g_http_server_stack,                         // Dedicated thread stack memory
        K_THREAD_STACK_SIZEOF(g_http_server_stack),  // Stack size in bytes (4096)
        &HttpServerTask::ThreadEntry,                // Static entry trampoline function
        this,                                        // User parameter 1 (p1: this pointer)
        nullptr,                                     // User parameter 2 (unused)
        nullptr,                                     // User parameter 3 (unused)
        kThreadPriority,                             // Thread priority (8)
        0U,                                          // Options
        K_NO_WAIT);                                  // Start scheduling immediately

    if (tid == nullptr) {
        LOG_ERR("Failed to create HTTP server thread");
        return Status::kInternalError;
    }

    k_thread_name_set(tid, "http_server");
    is_started_ = true;
    LOG_INF("HTTP server thread spawned on port %u", kPort);
    return Status::kOk;
}

void HttpServerTask::ThreadEntry(void* const p1, void* const /*p2*/, void* const /*p3*/) noexcept
{
    if (p1 == nullptr) {
        return;
    }
    auto* const self = static_cast<HttpServerTask*>(p1);
    self->Run();
}

void HttpServerTask::Run() noexcept
{
    LOG_INF("HTTP server listening thread starting on port %u...", kPort);

    // 1. Create IPv4 TCP socket
    const int server_sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        return;
    }

    // 2. Bind to INADDR_ANY:80
    struct sockaddr_in bind_addr {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(kPort);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    const int bind_res = zsock_bind(
        server_sock,
        reinterpret_cast<struct sockaddr*>(&bind_addr),
        sizeof(bind_addr));

    if (bind_res < 0) {
        LOG_ERR("Failed to bind socket: %d", errno);
        zsock_close(server_sock);
        return;
    }

    // 3. Listen for connections (backlog queue: 4)
    const int listen_res = zsock_listen(server_sock, 4);
    if (listen_res < 0) {
        LOG_ERR("Failed to listen on socket: %d", errno);
        zsock_close(server_sock);
        return;
    }

    LOG_INF("HTTP server is ready and accepting browser connections on port %u", kPort);

    // 4. Accept loop
    while (true) {
        struct sockaddr_in client_addr {};
        socklen_t client_addr_len = sizeof(client_addr);

        const int client_sock = zsock_accept(
            server_sock,
            reinterpret_cast<struct sockaddr*>(&client_addr),
            &client_addr_len);

        if (client_sock < 0) {
            LOG_WRN("Accept error: %d", errno);
            k_msleep(100);
            continue;
        }

        // Set 3-second receive timeout on client socket to prevent hung connections
        struct timeval timeout {};
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        zsock_setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Process request
        HandleClient(client_sock);

        // Gracefully shutdown transmitter (sends TCP FIN) before closing socket descriptor
        (void)zsock_shutdown(client_sock, ZSOCK_SHUT_WR);
        k_msleep(10);
        zsock_close(client_sock);
    }
}

void HttpServerTask::HandleClient(const int client_sock) noexcept
{
    char request_buf[256] = {};
    const ssize_t bytes_read = zsock_recv(client_sock, request_buf, sizeof(request_buf) - 1U, 0);
    if (bytes_read <= 0) {
        return;
    }
    request_buf[bytes_read] = '\0';

    // Route: Fast 404 for favicon to prevent browser stalling on missing icon
    if (strstr(request_buf, "GET /favicon.ico") != nullptr) {
        static const char k404[] =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        (void)SendAll(client_sock, k404, sizeof(k404) - 1U);
        return;
    }

    // Route: Telemetry JSON API
    if (strstr(request_buf, "GET /api/data") != nullptr) {
        const sensor::ImuMeasurement m = data_hub_.GetLatest();

        char json_body[192] = {};
        const int json_len = snprintf(
            json_body,
            sizeof(json_body),
            "{\"accel\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
            "\"gyro\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},"
            "\"temp\":%.2f,\"timestamp_ms\":%u}",
            m.accel_x, m.accel_y, m.accel_z,
            m.gyro_x, m.gyro_y, m.gyro_z,
            m.temperature,
            static_cast<unsigned int>(m.timestamp_ms));

        char response[384] = {};
        const int resp_len = snprintf(
            response,
            sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Connection: close\r\n\r\n"
            "%s",
            json_len,
            json_body);

        (void)SendAll(client_sock, response, static_cast<size_t>(resp_len));
        return;
    }

    // Route: HTML Web Dashboard
    const size_t html_len = strlen(kHtmlIndex);

    char header[160] = {};
    const int header_len = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        static_cast<int>(html_len));

    (void)SendAll(client_sock, header, static_cast<size_t>(header_len));
    (void)SendAll(client_sock, kHtmlIndex, html_len);
}

}  // namespace app::http

