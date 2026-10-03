#pragma once
#include "core.hpp"
#include <windows.h>
#include <functional>
#include <string>

namespace efp {
using Log = std::function<void(const std::string&)>;
class GameAdapter {
public:
    bool initialize(const Log& log, bool camera_writes = false);
    int camera_mode() const;
    bool camera_valid() const; // structural eligibility, not proven player ownership
    Context context() const;
private:
    std::uintptr_t base_ = 0;
    bool supported_ = false;
};
std::string executable_sha256();
} // namespace efp
