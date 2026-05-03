#pragma once

#include "LC3/lc3_gui_adapter.h"
#include "cell_bounds.h"

#include <FL/Fl_Table_Row.H>

#include <vector>

class MemoryTable : public Fl_Table_Row {
public:
    MemoryTable(int x, int y, int width, int height);

    void setRows(const std::vector<lc3::MemoryRow>& memory_rows);
    void fitColumns(int width);
    bool addressAtRow(int row, int& address) const;
    bool valueAtRow(int row, int& value) const;
    bool cellBounds(int row, int col, int& x, int& y, int& width, int& height);

private:
    void draw_cell(TableContext context, int row, int col, int x, int y, int width, int height) override;

    static const char* columnName(int col);
    static void drawHeaderCell(const char* text, int x, int y, int width, int height);
    void drawValueCell(int row, int col, int x, int y, int width, int height) const;
    static std::string valueText(const lc3::MemoryRow& memory, int col);

    std::vector<lc3::MemoryRow> rows_;
    std::vector<CellBounds> visible_cell_bounds_;
};
