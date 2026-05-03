#include "memory_table.h"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <array>

MemoryTable::MemoryTable(int x, int y, int width, int height)
    : Fl_Table_Row(x, y, width, height) {
    rows(0);
    cols(4);
    col_header(1);
    col_header_height(24);
    row_header(0);
    col_resize(1);
    row_resize(0);
    row_height_all(22);
    end();
}

void MemoryTable::setRows(const std::vector<lc3::MemoryRow>& memory_rows) {
    rows_ = memory_rows;
    Fl_Table_Row::rows(static_cast<int>(rows_.size()));
    row_height_all(22);
    visible_cell_bounds_.clear();
    redraw();
}

void MemoryTable::fitColumns(int width) {
    visible_cell_bounds_.clear();
    int flag_width = 62;
    int address_width = 86;
    int hex_width = 78;
    int binary_width = std::max(150, width - flag_width - address_width - hex_width - 20);
    col_width(0, flag_width);
    col_width(1, address_width);
    col_width(2, hex_width);
    col_width(3, binary_width);
}

bool MemoryTable::addressAtRow(int row, int& address) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return false;
    }
    address = rows_[static_cast<std::size_t>(row)].address;
    return true;
}

bool MemoryTable::valueAtRow(int row, int& value) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return false;
    }
    value = rows_[static_cast<std::size_t>(row)].value;
    return true;
}

bool MemoryTable::cellBounds(int row, int col, int& x, int& y, int& width, int& height) {
    if (find_cell(CONTEXT_CELL, row, col, x, y, width, height) != 0) {
        return true;
    }
    return lookupCellBounds(visible_cell_bounds_, row, col, x, y, width, height);
}

bool MemoryTable::scrollAddressNearUpperMiddle(int address) {
    int row = rowIndexForAddress(address);
    if (row < 0) {
        return false;
    }

    int visible_rows = visibleRowCapacity();
    int desired_offset = std::max(1, visible_rows / 3);
    int top_row = std::max(0, row - desired_offset);
    row_position(top_row);
    return true;
}

int MemoryTable::handle(int event) {
    if (event == FL_MOUSEWHEEL && !Fl::event_inside(this)) {
        return 0;
    }
    return Fl_Table_Row::handle(event);
}

void MemoryTable::draw_cell(TableContext context, int row, int col, int x, int y, int width, int height) {
    switch (context) {
        case CONTEXT_STARTPAGE:
            visible_cell_bounds_.clear();
            fl_font(FL_HELVETICA, 13);
            return;
        case CONTEXT_COL_HEADER:
            drawHeaderCell(columnName(col), x, y, width, height);
            return;
        case CONTEXT_CELL:
            rememberCellBounds(visible_cell_bounds_, row, col, x, y, width, height);
            drawValueCell(row, col, x, y, width, height);
            return;
        default:
            return;
    }
}

const char* MemoryTable::columnName(int col) {
    static const std::array<const char*, 4> names = { "Flag", "Address", "Hex", "Binary" };
    return (col >= 0 && col < static_cast<int>(names.size())) ? names[static_cast<std::size_t>(col)] : "";
}

void MemoryTable::drawHeaderCell(const char* text, int x, int y, int width, int height) {
    fl_push_clip(x, y, width, height);
    fl_draw_box(FL_THIN_UP_BOX, x, y, width, height, fl_rgb_color(232, 236, 240));
    fl_color(FL_BLACK);
    fl_font(FL_HELVETICA_BOLD, 12);
    fl_draw(text, x + 5, y, width - 10, height, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
    fl_pop_clip();
}

void MemoryTable::drawValueCell(int row, int col, int x, int y, int width, int height) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return;
    }

    const lc3::MemoryRow& memory = rows_[static_cast<std::size_t>(row)];
    fl_push_clip(x, y, width, height);
    Fl_Color background = (row % 2 == 0) ? FL_WHITE : fl_rgb_color(248, 250, 252);
    if (memory.is_pc) {
        background = fl_rgb_color(255, 245, 204);
    }
    fl_color(background);
    fl_rectf(x, y, width, height);
    fl_color(fl_rgb_color(214, 220, 226));
    fl_rect(x, y, width, height);
    fl_color(FL_BLACK);
    fl_font(col == 3 ? FL_COURIER : FL_HELVETICA, 13);
    std::string text = valueText(memory, col);
    fl_draw(text.c_str(), x + 5, y, width - 10, height, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
    fl_pop_clip();
}

std::string MemoryTable::valueText(const lc3::MemoryRow& memory, int col) {
    switch (col) {
        case 0:
            if (memory.is_pc && memory.has_breakpoint) return "PC BP";
            if (memory.is_pc) return "PC";
            if (memory.has_breakpoint) return "BP";
            return "";
        case 1: return lc3::formatHexWord(memory.address);
        case 2: return lc3::formatHexWord(memory.value);
        case 3: return lc3::formatBinaryWord(memory.value);
        default: return "";
    }
}

int MemoryTable::rowIndexForAddress(int address) const {
    address &= 0xFFFF;
    for (int row = 0; row < static_cast<int>(rows_.size()); row++) {
        if ((rows_[static_cast<std::size_t>(row)].address & 0xFFFF) == address) {
            return row;
        }
    }
    return -1;
}

int MemoryTable::visibleRowCapacity() {
    if (rows_.empty()) {
        return 1;
    }

    int row_height_px = std::max(1, row_height(0));
    int body_height = h() - col_header_height();
    if (body_height <= 0) {
        return 1;
    }
    return std::max(1, body_height / row_height_px);
}
