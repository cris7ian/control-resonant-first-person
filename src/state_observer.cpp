#include "state_observer.hpp"
#include "game_layout.hpp"
#include <MinHook.h>
#include <atomic>
#include <array>
#include <cstring>
#include <mutex>

namespace efp {
namespace {
using MirrorSystem = void (*)(void* facts, void* stacks);
using layout::mirror_rva;
using layout::mirror_prefix;
struct Observer {
    MirrorSystem original{};
    std::atomic<Millis> last_sample{0};
    std::mutex mutex;
    StateSnapshot snapshot;
    Millis observed_at = 0;
};
// Process lifetime; deliberately never hot-unloaded or destroyed under live callbacks.
std::atomic<Observer*> observer{nullptr};
bool memory_read(std::uintptr_t address, void* out, std::size_t size) {
    SIZE_T got{};
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &got) && got == size;
}
void mirror_detour(void* facts, void* stacks) {
    auto* state = observer.load(std::memory_order_acquire);
    state->original(facts, stacks); // identity call: all original behavior preserved
    const auto now = GetTickCount64();
    auto previous = state->last_sample.load(std::memory_order_relaxed);
    if (now - previous < 50 || !state->last_sample.compare_exchange_strong(previous, now)) return;
    try {
        auto sample = read_state_snapshot(reinterpret_cast<std::uintptr_t>(stacks), memory_read);
        std::unique_lock lock(state->mutex, std::try_to_lock);
        if (lock.owns_lock() && now >= state->observed_at) { state->snapshot = std::move(sample); state->observed_at = now; }
    } catch (...) {
        // No camera writes occur even if diagnostic allocation/read fails.
        std::unique_lock lock(state->mutex, std::try_to_lock);
        if (lock.owns_lock() && now >= state->observed_at) { state->snapshot = {}; state->observed_at = now; }
    }
}
}
bool start_state_observer(std::uintptr_t base, const Log& log) {
    if (observer.load()) return true;
    auto* target = reinterpret_cast<void*>(base + mirror_rva);
    std::array<unsigned char, mirror_prefix.size()> actual{};
    if (!memory_read(reinterpret_cast<std::uintptr_t>(target), actual.data(), actual.size()) || actual != mirror_prefix) {
        log("State observer signature mismatch or earlier hook: no state detour installed."); return false;
    }
    const auto initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) {
        log("State observer: MinHook initialization failed."); return false;
    }
    auto* state = new Observer;
    const auto created = MH_CreateHook(target, reinterpret_cast<void*>(&mirror_detour), reinterpret_cast<void**>(&state->original));
    if (created != MH_OK) { delete state; log(std::string("State observer hook creation failed: ") + MH_StatusToString(created)); return false; }
    observer.store(state, std::memory_order_release); // publish trampoline before enabling
    const auto enabled = MH_EnableHook(target);
    if (enabled != MH_OK) {
        // Keep observer storage alive even on a partial enable failure.
        log(std::string("State observer hook enable failed: ") + MH_StatusToString(enabled)); return false;
    }
    log("Diagnostic identity state observer installed at RVA 0x17D8E10; sampling UI stacks after original updates.");
    return true;
}
StateSnapshot latest_state_snapshot(Millis& observed_at) {
    auto* state = observer.load(std::memory_order_acquire);
    if (!state) { observed_at = 0; return {}; }
    std::lock_guard lock(state->mutex);
    observed_at = state->observed_at;
    return state->snapshot;
}
} // namespace efp
