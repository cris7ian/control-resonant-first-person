#include "game_adapter.hpp"
#include "state_observer.hpp"
#include "camera_override.hpp"
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>

namespace efp {
namespace {
constexpr char supported_hash[] = "4f6596b08bb5bc7fe4150cf5b9f71d7bae87eea02d66627c03a5e05ffe84ea62";
constexpr std::uintptr_t mode_rva = 0x5D05058;
constexpr std::array<unsigned char, 25> camera_prologue = {
    0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20,0x53,0x56,0x57,0x41,0x54,0x41,
    0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0x90,0x03,0x00,0x00};
bool read_memory(std::uintptr_t address, void* out, std::size_t size) {
    SIZE_T read{};
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &read) && read == size;
}
}
std::string executable_sha256() {
    std::array<wchar_t, 32768> filename{};
    const auto length = GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
    if (!length || length >= filename.size()) return {};
    std::ifstream input(std::filesystem::path(filename.data()), std::ios::binary);
    if (!input) return {};
    BCRYPT_ALG_HANDLE algorithm{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    BCRYPT_HASH_HANDLE hash{};
    DWORD bytes{}, object_size{};
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &bytes, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0); return {};
    }
    std::vector<unsigned char> object(object_size);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0); return {};
    }
    std::array<char, 65536> buffer{};
    bool ok = true;
    while (input) {
        input.read(buffer.data(), buffer.size());
        const auto count = input.gcount();
        if (count && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0) < 0) { ok = false; break; }
    }
    if (!input.eof()) ok = false;
    std::array<unsigned char, 32> result{};
    if (ok) ok = BCryptFinishHash(hash, result.data(), static_cast<ULONG>(result.size()), 0) >= 0;
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok) return {};
    std::ostringstream text;
    text << std::hex << std::setfill('0');
    for (auto byte : result) text << std::setw(2) << static_cast<unsigned>(byte);
    return text.str();
}
bool GameAdapter::initialize(const Log& log, bool camera_writes) {
    base_ = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto hash = executable_sha256();
    log("Executable SHA256: " + (hash.empty() ? std::string("unavailable") : hash));
    if (hash != supported_hash) { log("Unsupported executable: native observations and all camera changes disabled."); return false; }
    IMAGE_DOS_HEADER dos{};
    if (!read_memory(base_, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) return false;
    IMAGE_NT_HEADERS64 nt{};
    if (!read_memory(base_ + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
        nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.SizeOfImage <= mode_rva + sizeof(int)) return false;
    const auto sections_address = base_ + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    std::size_t matches = 0;
    std::uintptr_t candidate = 0;
    bool mode_section_ok = false;
    for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section{};
        if (!read_memory(sections_address + i * sizeof(section), &section, sizeof(section))) return false;
        const auto size = section.Misc.VirtualSize;
        if (section.VirtualAddress > nt.OptionalHeader.SizeOfImage || size > nt.OptionalHeader.SizeOfImage - section.VirtualAddress) return false;
        if (mode_rva >= section.VirtualAddress && mode_rva + sizeof(int) <= section.VirtualAddress + size) {
            mode_section_ok = (section.Characteristics & IMAGE_SCN_MEM_WRITE) && !(section.Characteristics & IMAGE_SCN_MEM_EXECUTE);
        }
        if (std::memcmp(section.Name, ".text", 5) != 0) continue;
        std::vector<unsigned char> text(size);
        if (!read_memory(base_ + section.VirtualAddress, text.data(), size)) return false;
        for (std::size_t at = 0; at + camera_prologue.size() <= text.size(); ++at) {
            if (std::memcmp(text.data() + at, camera_prologue.data(), camera_prologue.size()) == 0) {
                ++matches; candidate = section.VirtualAddress + at;
            }
        }
    }
    std::ostringstream message;
    message << "Camera prologue candidates: " << matches << "; candidate RVA 0x" << std::hex << candidate;
    log(message.str());
    supported_ = matches == 1 && candidate == 0x207BF90 && mode_section_ok;
    log(supported_ ? "Static baseline matched. Exclusive camera ownership and head/eye attachment remain unverified." : "Static baseline rejected; adapter stays inactive.");
    if (supported_) {
        start_state_observer(base_, log);
        if (camera_writes) start_camera_override(base_, log);
    }
    return supported_;
}
int GameAdapter::camera_mode() const {
    int mode = -1;
    if (!supported_ || !read_memory(base_ + mode_rva, &mode, sizeof(mode))) return -1;
    return mode;
}
bool GameAdapter::camera_valid() const { return supported_ && camera_record_recent(); }
Context GameAdapter::context() const {
    Context result;
    result.camera_valid = camera_valid();
    Millis observed{};
    const auto sample = latest_state_snapshot(observed);
    const auto now = GetTickCount64();
    result.state_observed = observed;
    result.state_fresh = observed != 0 && now >= observed && now - observed <= 150;
    if (result.state_fresh) result.state = diagnostic_state(sample, camera_mode());
    // Structural eligibility is not proof of exclusive player-camera ownership.
    return result;
}
} // namespace efp
