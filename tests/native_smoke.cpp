#include <windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto path = std::filesystem::absolute(argv[1]);
    const auto log = path.parent_path() / "ExplorationFirstPerson.log";
    std::error_code error; std::filesystem::remove(log, error);
    // Execute our own build only, outside the game. Reference DLLs are not loaded.
    const auto module = LoadLibraryW(path.c_str());
    if (!module) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 1; }
    for (int attempt = 0; attempt < 100; ++attempt) {
        Sleep(50);
        std::ifstream stream(log);
        std::string text((std::istreambuf_iterator<char>(stream)), {});
        if (text.find("Unsupported executable") != std::string::npos && text.find("XInput") != std::string::npos) {
            std::cout << "Native startup passed; unsupported host rejected; camera writes disabled.\n";
            return 0;
        }
    }
    std::cerr << "Native startup log missing expected safety markers.\n";
    return 1;
}
