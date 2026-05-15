#pragma once

#include "LC3/lc3_gui_adapter.h"
#include "cell_bounds.h"

#include <FL/Fl_Table_Row.H>

#include <string>
#include <vector>

class RegisterTable : public Fl_Table_Row {
public:
    RegisterTable(int x, int y, int width, int height);

    void setRegisters(const lc3::RegisterView& registers);
    void fitColumns(int width);
    bool editableRow(int row) const;
    bool cellBounds(int row, int col, int& x, int& y, int& width, int& height);
    std::string nameAtRow(int row) const;
    std::string valueAtRow(int row) const;

private:
    void draw_cell(TableContext context, int row, int col, int x, int y, int width, int height) override;

    static void drawHeaderCell(const char* text, int x, int y, int width, int height);
    void drawValueCell(int row, int col, int x, int y, int width, int height) const;
    static std::string registerName(int row);
    std::string registerValue(int row) const;

    lc3::RegisterView registers_;
    std::vector<CellBounds> visible_cell_bounds_;
};
