#pragma once

#include <filesystem>
#include <string>

namespace ui {

std::filesystem::path utf8Path(const char* text);
std::string readFile(const std::filesystem::path& path);
void writeFile(const std::filesystem::path& path, const std::string& text);
bool parseBoolText(const std::string& text, bool& value);

} // namespace ui
