#include "file_utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace ui {
namespace {

std::string lowercaseAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

} // namespace

std::filesystem::path utf8Path(const char* text) {
    if (!text) {
        return {};
    }
#if defined(__cpp_lib_char8_t) && (__cpp_lib_char8_t >= 201907L)
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(text),
                                                    std::char_traits<char>::length(text)));
#else
    return std::filesystem::u8path(text);
#endif
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open file for reading: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open file for writing: " + path.string());
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error("Failed while writing file: " + path.string());
    }
}

bool parseBoolText(const std::string& text, bool& value) {
    std::string lower = lowercaseAscii(text);
    lower.erase(std::remove_if(lower.begin(), lower.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }), lower.end());

    if (lower == "1" || lower == "true" || lower == "yes" || lower == "y") {
        value = true;
        return true;
    }
    if (lower == "0" || lower == "false" || lower == "no" || lower == "n") {
        value = false;
        return true;
    }
    return false;
}

} // namespace ui
