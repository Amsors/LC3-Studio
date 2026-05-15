#pragma once

#include <vector>

struct CellBounds {
    int row = -1;
    int col = -1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline void rememberCellBounds(std::vector<CellBounds>& cells,
                               int row,
                               int col,
                               int x,
                               int y,
                               int width,
                               int height) {
    for (CellBounds& cell : cells) {
        if (cell.row == row && cell.col == col) {
            cell.x = x;
            cell.y = y;
            cell.width = width;
            cell.height = height;
            return;
        }
    }
    cells.push_back({ row, col, x, y, width, height });
}

inline bool lookupCellBounds(const std::vector<CellBounds>& cells,
                             int row,
                             int col,
                             int& x,
                             int& y,
                             int& width,
                             int& height) {
    for (const CellBounds& cell : cells) {
        if (cell.row == row && cell.col == col && cell.width > 0 && cell.height > 0) {
            x = cell.x;
            y = cell.y;
            width = cell.width;
            height = cell.height;
            return true;
        }
    }
    return false;
}
