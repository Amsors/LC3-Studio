#define LC3_AS_LIBRARY
#include "assembler.cpp"

#include "lc3_gui_adapter.h"

#include <filesystem>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace lc3 {

namespace {

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

std::string findConfigPath() {
    std::filesystem::path exe_dir = executableDirectory();
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "LC3" / "config.json",
        std::filesystem::current_path() / "config.json",
        exe_dir / "LC3" / "config.json",
        exe_dir / "config.json",
        exe_dir / ".." / ".." / ".." / "LC3" / "config.json"
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return candidate.lexically_normal().string();
        }
    }
    return "LC3/config.json";
}

} // namespace

AssembleResult AssemblerService::assembleSource(const std::string& source) const {
    AssembleResult result;
    try {
        Parser parser(findConfigPath());
        result.words = parser.AssembleSourceToWords(source);

        std::ostringstream out;
        for (const std::string& word : result.words) {
            out << word << '\n';
        }
        result.machine_code = out.str();
        result.ok = true;
    } catch (const std::exception& e) {
        result.ok = false;
        result.error_message = e.what();
    }
    return result;
}

} // namespace lc3
