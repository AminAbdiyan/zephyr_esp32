#pragma once

#include <stdint.h>

namespace app {

enum class Status : int32_t {
    kOk = 0,
    kInvalidArgument,
    kNotReady,
    kTimeout,
    kBusy,
    kIoError,
    kInternalError,
};

[[nodiscard]] constexpr bool IsOk(const Status status) noexcept {
    return status == Status::kOk;
}

}  // namespace app

