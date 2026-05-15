#pragma once

#include <FL/Fl_Text_Display.H>

#include <string>

namespace ui {

constexpr char kAsmStyleDefault = 'A';

extern const Fl_Text_Display::Style_Table_Entry kAsmStyleTable[];
extern const int kAsmStyleTableSize;

std::string buildAsmStyleText(const std::string& text);

} // namespace ui
