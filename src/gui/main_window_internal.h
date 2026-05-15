#pragma once

#include "src/gui/main_window.h"
#include "src/examples/embedded_examples.h"
#include "src/gui/widgets/asm_highlighter.h"
#include "src/gui/io/file_utils.h"
#include "src/gui/widgets/memory_table.h"
#include "src/gui/widgets/register_table.h"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace gui::main_window_detail {


constexpr int kMenuHeight = 28;
constexpr int kToolbarHeight = 86;
constexpr int kStatusHeight = 26;
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kLabelHeight = 22;
constexpr int kButtonHeight = 30;
constexpr int kToolbarRowGap = 6;
constexpr int kTabHeaderHeight = 28;
constexpr int kMemoryRowsBeforePc = 8;
constexpr int kMemoryRowsAfterPc = 32;
constexpr int kDefaultRunRateLimit = 5000;
constexpr int kMinRunRateLimit = 1;
constexpr int kMaxRunRateLimit = 50000;
constexpr double kRunTimerBaseSeconds = 0.02;

class EnterCallbackInput : public Fl_Input {
public:
    using Fl_Input::Fl_Input;

    int handle(int event) override {
        if (event == FL_KEYDOWN) {
            int key = Fl::event_key();
            bool command_modifier = (Fl::event_state() & (FL_CTRL | FL_ALT | FL_META)) != 0;
            if (!command_modifier && (key == FL_Enter || key == FL_KP_Enter)) {
                do_callback();
                insert_position(size());
                return 1;
            }
        }
        return Fl_Input::handle(event);
    }
};

class WheelIsolatedTextEditor : public Fl_Text_Editor {
public:
    using Fl_Text_Editor::Fl_Text_Editor;

    int handle(int event) override {
        if (event == FL_MOUSEWHEEL) {
            Fl_Text_Editor::handle(event);
            return 1;
        }
        return Fl_Text_Editor::handle(event);
    }
};

class AsmSourceEditor : public WheelIsolatedTextEditor {
public:
    using WheelIsolatedTextEditor::WheelIsolatedTextEditor;

    int handle(int event) override {
        if (event == FL_KEYDOWN) {
            int key = Fl::event_key();
            int state = Fl::event_state();
            bool command_modifier = (state & (FL_CTRL | FL_ALT | FL_META)) != 0;
            if (!command_modifier && (key == FL_Enter || key == FL_KP_Enter)) {
                return insertNewlineWithIndent();
            }
            if (!command_modifier && key == FL_Tab && (state & FL_SHIFT)) {
                return outdentOneLevel();
            }
        }
        return WheelIsolatedTextEditor::handle(event);
    }

private:
    static constexpr int kIndentSpaces = 4;

    std::string currentLineIndent(int pos) const {
        Fl_Text_Buffer* text_buffer = buffer();
        if (!text_buffer) {
            return "";
        }

        int line_start = text_buffer->line_start(pos);
        int line_end = text_buffer->line_end(pos);
        int indent_end = line_start;
        while (indent_end < line_end) {
            char c = text_buffer->byte_at(indent_end);
            if (c != ' ' && c != '\t') {
                break;
            }
            indent_end++;
        }

        if (indent_end <= line_start) {
            return "";
        }

        char* text = text_buffer->text_range(line_start, indent_end);
        std::string indent = text ? text : "";
        std::free(text);
        return indent;
    }

    int insertNewlineWithIndent() {
        Fl_Text_Buffer* text_buffer = buffer();
        if (!text_buffer) {
            return 0;
        }

        int replace_start = insert_position();
        int replace_end = replace_start;
        if (text_buffer->selected()) {
            text_buffer->selection_position(&replace_start, &replace_end);
        }

        std::string inserted_text = "\n" + currentLineIndent(replace_start);
        text_buffer->replace(replace_start, replace_end, inserted_text.c_str());
        text_buffer->unselect();
        insert_position(replace_start + static_cast<int>(inserted_text.size()));
        show_insert_position();
        return 1;
    }

    int removableIndentAt(int line_start) const {
        Fl_Text_Buffer* text_buffer = buffer();
        if (!text_buffer || line_start >= text_buffer->length()) {
            return 0;
        }

        char first = text_buffer->byte_at(line_start);
        if (first == '\t') {
            return 1;
        }
        if (first != ' ') {
            return 0;
        }

        int line_end = text_buffer->line_end(line_start);
        int count = 0;
        while (line_start + count < line_end && count < kIndentSpaces &&
               text_buffer->byte_at(line_start + count) == ' ') {
            count++;
        }
        return count;
    }

    static void adjustPositionAfterDelete(int delete_start, int delete_count, int& pos) {
        if (delete_count <= 0 || delete_start >= pos) {
            return;
        }
        if (delete_start + delete_count <= pos) {
            pos -= delete_count;
        } else {
            pos = delete_start;
        }
    }

    int outdentOneLevel() {
        Fl_Text_Buffer* text_buffer = buffer();
        if (!text_buffer) {
            return 0;
        }

        int selection_start = 0;
        int selection_end = 0;
        bool has_selection = text_buffer->selected() &&
                             text_buffer->selection_position(&selection_start, &selection_end);
        int range_start = has_selection ? selection_start : insert_position();
        int range_end = has_selection ? selection_end : range_start;
        int last_pos = range_end;
        if (has_selection && last_pos > range_start &&
            text_buffer->byte_at(last_pos - 1) == '\n') {
            last_pos--;
        }

        std::vector<int> line_starts;
        int line_start = text_buffer->line_start(range_start);
        while (true) {
            line_starts.push_back(line_start);
            int line_end = text_buffer->line_end(line_start);
            if (line_end >= last_pos || line_end >= text_buffer->length()) {
                break;
            }
            line_start = line_end + 1;
        }

        int new_insert_position = insert_position();
        int new_selection_start = selection_start;
        int new_selection_end = selection_end;
        bool changed = false;

        for (auto it = line_starts.rbegin(); it != line_starts.rend(); ++it) {
            int delete_start = *it;
            int delete_count = removableIndentAt(delete_start);
            if (delete_count <= 0) {
                continue;
            }
            text_buffer->remove(delete_start, delete_start + delete_count);
            adjustPositionAfterDelete(delete_start, delete_count, new_insert_position);
            if (has_selection) {
                adjustPositionAfterDelete(delete_start, delete_count, new_selection_start);
                adjustPositionAfterDelete(delete_start, delete_count, new_selection_end);
            }
            changed = true;
        }

        if (has_selection) {
            text_buffer->select(new_selection_start, new_selection_end);
            insert_position(new_selection_end);
        } else {
            text_buffer->unselect();
            insert_position(new_insert_position);
        }
        if (changed) {
            show_insert_position();
        }
        return 1;
    }
};

class WheelIsolatedTextDisplay : public Fl_Text_Display {
public:
    using Fl_Text_Display::Fl_Text_Display;

    int handle(int event) override {
        if (event == FL_MOUSEWHEEL) {
            Fl_Text_Display::handle(event);
            return 1;
        }
        return Fl_Text_Display::handle(event);
    }
};

inline std::string exampleMenuTitle(const embedded_examples::AssemblyExample& example) {
    std::string title = example.title && *example.title ? example.title : example.id;
    if (title.empty()) {
        title = "Example";
    }
    std::replace(title.begin(), title.end(), '/', '-');
    return title;
}

inline bool parseRunRateText(const char* text, int& value) {
    if (!text) {
        return false;
    }

    std::istringstream input(text);
    long long parsed = 0;
    char trailing = '\0';
    if (!(input >> parsed)) {
        return false;
    }
    if (input >> trailing) {
        return false;
    }
    if (parsed < kMinRunRateLimit || parsed > kMaxRunRateLimit) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}


} // namespace gui::main_window_detail
