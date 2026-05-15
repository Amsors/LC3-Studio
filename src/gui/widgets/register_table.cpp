#include "register_table.h"

#include <FL/Enumerations.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <array>

RegisterTable::RegisterTable(int x, int y, int width, int height)
    : Fl_Table_Row(x, y, width, height) {
    rows(14);
    cols(2);
    col_header(1);
    col_header_height(24);
    row_header(0);
    col_resize(1);
    row_resize(0);
    row_height_all(23);
    col_width(0, 86);
    col_width(1, 112);
    end();
}

void RegisterTable::setRegisters(const lc3::RegisterView& registers) {
    registers_ = registers;
    redraw();
}

void RegisterTable::fitColumns(int width) {
    visible_cell_bounds_.clear();
    int name_width = std::min(96, std::max(72, width * 42 / 100));
    col_width(0, name_width);
    col_width(1, std::max(70, width - name_width - 18));
}

bool RegisterTable::editableRow(int row) const {
    return (row >= 0 && row <= 10) || row == 13;
}

bool RegisterTable::cellBounds(int row, int col, int& x, int& y, int& width, int& height) {
    if (find_cell(CONTEXT_CELL, row, col, x, y, width, height) != 0) {
        return true;
    }
    return lookupCellBounds(visible_cell_bounds_, row, col, x, y, width, height);
}

std::string RegisterTable::nameAtRow(int row) const {
    return registerName(row);
}

std::string RegisterTable::valueAtRow(int row) const {
    return registerValue(row);
}

void RegisterTable::draw_cell(TableContext context, int row, int col, int x, int y, int width, int height) {
    switch (context) {
        case CONTEXT_STARTPAGE:
            visible_cell_bounds_.clear();
            fl_font(FL_HELVETICA, 13);
            return;
        case CONTEXT_COL_HEADER:
            drawHeaderCell(col == 0 ? "Name" : "Value", x, y, width, height);
            return;
        case CONTEXT_CELL:
            rememberCellBounds(visible_cell_bounds_, row, col, x, y, width, height);
            drawValueCell(row, col, x, y, width, height);
            return;
        default:
            return;
    }
}

void RegisterTable::drawHeaderCell(const char* text, int x, int y, int width, int height) {
    fl_push_clip(x, y, width, height);
    fl_draw_box(FL_THIN_UP_BOX, x, y, width, height, fl_rgb_color(232, 236, 240));
    fl_color(FL_BLACK);
    fl_font(FL_HELVETICA_BOLD, 12);
    fl_draw(text, x + 5, y, width - 10, height, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
    fl_pop_clip();
}

void RegisterTable::drawValueCell(int row, int col, int x, int y, int width, int height) const {
    fl_push_clip(x, y, width, height);
    Fl_Color background = (row % 2 == 0) ? FL_WHITE : fl_rgb_color(248, 250, 252);
    if (row == 8) {
        background = fl_rgb_color(232, 245, 255);
    }
    fl_color(background);
    fl_rectf(x, y, width, height);
    fl_color(fl_rgb_color(214, 220, 226));
    fl_rect(x, y, width, height);
    fl_color(FL_BLACK);
    fl_font(col == 0 ? FL_HELVETICA_BOLD : FL_COURIER, 13);
    std::string text = col == 0 ? registerName(row) : registerValue(row);
    fl_draw(text.c_str(), x + 5, y, width - 10, height, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
    fl_pop_clip();
}

std::string RegisterTable::registerName(int row) {
    if (row >= 0 && row < 8) {
        return "R" + std::to_string(row);
    }
    static const std::array<const char*, 6> names = { "PC", "IR", "CC", "STEPS", "RUNNING", "HALTED" };
    int index = row - 8;
    return index >= 0 && index < static_cast<int>(names.size()) ? names[static_cast<std::size_t>(index)] : "";
}

std::string RegisterTable::registerValue(int row) const {
    if (row >= 0 && row < 8) {
        return lc3::formatHexWord(registers_.r[row]);
    }
    switch (row) {
        case 8: return lc3::formatHexWord(registers_.pc);
        case 9: return lc3::formatHexWord(registers_.ir);
        case 10: return registers_.cc;
        case 11: return std::to_string(registers_.executed_instructions);
        case 12: return registers_.running ? "true" : "false";
        case 13: return registers_.halted ? "true" : "false";
        default: return "";
    }
}
