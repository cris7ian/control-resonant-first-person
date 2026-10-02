// Execute our detour against owned mock records only. No game process or reference DLL is loaded.
#include <windows.h>
#include "../src/camera_override.cpp"
#include <iostream>
#include <stdexcept>

namespace {
int calls = 0;
float* output{};
std::uintptr_t fake_original(void*, std::uintptr_t a2, std::uintptr_t a3, std::uintptr_t a4,
    std::uintptr_t a5, std::uintptr_t a6, std::uintptr_t a7, std::uintptr_t a8, std::uintptr_t a9, std::uintptr_t a10) {
    if (a2!=2 || a3!=3 || a4!=4 || a5!=5 || a6!=6 || a7!=7 || a8!=8 || a9!=9 || a10!=10) throw std::runtime_error("argument forwarding changed");
    ++calls;
    output[9] = 0; output[10] = 0; output[11] = 6;
    return 12345;
}
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    try {
        auto* records = static_cast<float*>(VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        check(records != nullptr,"allocation failed"); output = records;
        output[6] = 0; output[7] = 0; output[8] = -1;
        float input[12]{}; input[5] = -0.25f;
        alignas(16) unsigned char owner[0x70]{};
        const auto input_base = reinterpret_cast<std::uintptr_t>(input);
        const auto output_base = reinterpret_cast<std::uintptr_t>(output);
        std::memcpy(owner+0x30,&input_base,sizeof(input_base)); std::memcpy(owner+0x38,&output_base,sizeof(output_base));
        efp::original = &fake_original;
        efp::Settings settings; settings.debug = true;
        efp::publish_camera_control(false,GetTickCount64(),settings);
        auto result = efp::camera_detour(owner,2,3,4,5,6,7,8,9,10);
        check(result==12345 && calls==1,"original result/call count changed");
        check(output[9]==0 && output[10]==0 && output[11]==6,"inactive path changed native output");
        auto telemetry = efp::latest_camera_telemetry();
        check(telemetry.anchor_valid && !telemetry.anchor_used && !telemetry.write_attempted,"inactive path attempted placement");
        // Same valid output but a malformed input pair must leave original output intact.
        input[5] = 100;
        result = efp::camera_detour(owner,2,3,4,5,6,7,8,9,10);
        check(result==12345 && calls==2,"rejected anchor changed native call");
        check(output[9]==0 && output[10]==0 && output[11]==6,"rejected anchor wrote output");
        telemetry = efp::latest_camera_telemetry();
        check(!telemetry.anchor_valid && !telemetry.write_attempted,"invalid anchor did not fail closed");
        check(efp::last_valid_record.load()==0,"invalid anchor retained eligibility");
        VirtualFree(records,0,MEM_RELEASE);
        std::cout << "Owned-record original forwarding, inactive no-write, and invalid-anchor no-write passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
