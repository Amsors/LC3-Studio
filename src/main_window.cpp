#include "main_window.h"

#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace {

constexpr int kMenuHeight = 28;
constexpr int kToolbarHeight = 86;
constexpr int kStatusHeight = 26;
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kLabelHeight = 22;
constexpr int kButtonHeight = 30;
constexpr int kToolbarRowGap = 6;
constexpr int kMemoryRowsBeforePc = 8;
constexpr int kMemoryRowsAfterPc = 32;
constexpr int kRunStepsPerTick = 100;
constexpr double kRunTickSeconds = 0.02;

constexpr char kStyleDefault = 'A';
constexpr char kStyleInstruction = 'B';
constexpr char kStyleRegister = 'C';
constexpr char kStyleImmediate = 'D';
constexpr char kStyleLabel = 'E';
constexpr char kStyleComment = 'F';

const Fl_Text_Display::Style_Table_Entry kAsmStyleTable[] = {
    { FL_BLACK, FL_COURIER, 14, 0, 0 },
    { fl_rgb_color(20, 74, 145), FL_COURIER_BOLD, 14, 0, 0 },
    { fl_rgb_color(174, 43, 43), FL_COURIER_BOLD, 14, 0, 0 },
    { fl_rgb_color(30, 122, 67), FL_COURIER, 14, 0, 0 },
    { fl_rgb_color(116, 70, 168), FL_COURIER, 14, 0, 0 },
    { fl_rgb_color(113, 119, 124), FL_COURIER_ITALIC, 14, 0, 0 },
};

struct AsmToken {
    std::size_t start = 0;
    std::size_t end = 0;
    std::string lower_text;
};

std::filesystem::path utf8Path(const char* text) {
    return text ? std::filesystem::u8path(text) : std::filesystem::path();
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

std::string lowercaseAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

bool isRegisterToken(const std::string& token) {
    return token.size() == 2 && token[0] == 'r' && token[1] >= '0' && token[1] <= '7';
}

bool hasValidDigits(const std::string& token, std::size_t begin, int base) {
    if (begin >= token.size()) {
        return false;
    }
    if (token[begin] == '-') {
        begin++;
        if (begin >= token.size()) {
            return false;
        }
    }

    for (std::size_t i = begin; i < token.size(); i++) {
        unsigned char c = static_cast<unsigned char>(token[i]);
        switch (base) {
            case 2:
                if (c != '0' && c != '1') return false;
                break;
            case 8:
                if (c < '0' || c > '7') return false;
                break;
            case 10:
                if (!std::isdigit(c)) return false;
                break;
            case 16:
                if (!std::isxdigit(c)) return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

bool isImmediateToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        return true;
    }
    switch (token[0]) {
        case '#': return hasValidDigits(token, 1, 10);
        case 'x': return hasValidDigits(token, 1, 16);
        case 'b': return hasValidDigits(token, 1, 2);
        case 'o': return hasValidDigits(token, 1, 8);
        case '-': return hasValidDigits(token, 0, 10);
        default:
            return std::isdigit(static_cast<unsigned char>(token[0])) &&
                   hasValidDigits(token, 0, 10);
    }
}

bool isInstructionToken(const std::string& token) {
    static const std::unordered_set<std::string> instructions = {
        "add", "and", "br", "brn", "brz", "brp", "brnz", "brnp", "brzp", "brnzp",
        "jmp", "jsr", "jsrr", "ld", "ldi", "ldr", "lea", "not", "ret", "rti",
        "st", "sti", "str", "trap", "getc", "out", "puts", "in", "putsp", "halt",
        ".orig", ".fill", ".stringz", ".blkw", ".end"
    };
    return instructions.find(token) != instructions.end();
}

bool isLabelToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    unsigned char first = static_cast<unsigned char>(token[0]);
    if (!(std::isalpha(first) || token[0] == '_')) {
        return false;
    }
    for (char c : token) {
        unsigned char ch = static_cast<unsigned char>(c);
        if (!(std::isalnum(ch) || c == '_')) {
            return false;
        }
    }
    return !isRegisterToken(token) && !isInstructionToken(token);
}

void applyStyle(std::string& styles, std::size_t start, std::size_t end, char style) {
    if (start >= styles.size()) {
        return;
    }
    end = std::min(end, styles.size());
    std::fill(styles.begin() + static_cast<std::ptrdiff_t>(start),
              styles.begin() + static_cast<std::ptrdiff_t>(end),
              style);
}

std::size_t findCommentStart(const std::string& text, std::size_t begin, std::size_t end) {
    bool in_string = false;
    bool escape = false;
    for (std::size_t i = begin; i < end; i++) {
        char c = text[i];
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == ';') {
            return i;
        }
    }
    return end;
}

std::vector<AsmToken> tokenizeAsmCode(const std::string& text, std::size_t begin, std::size_t end) {
    std::vector<AsmToken> tokens;
    std::size_t pos = begin;
    while (pos < end) {
        while (pos < end && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == ',')) {
            pos++;
        }
        if (pos >= end) {
            break;
        }

        std::size_t token_start = pos;
        if (text[pos] == '"') {
            pos++;
            bool escape = false;
            while (pos < end) {
                char c = text[pos++];
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    break;
                }
            }
        } else {
            while (pos < end && text[pos] != ' ' && text[pos] != '\t' &&
                   text[pos] != ',' && text[pos] != '"') {
                pos++;
            }
        }

        tokens.push_back({ token_start, pos, lowercaseAscii(text.substr(token_start, pos - token_start)) });
    }
    return tokens;
}

std::string buildAsmStyleText(const std::string& text) {
    std::string styles(text.size(), kStyleDefault);
    std::size_t line_begin = 0;
    while (line_begin < text.size()) {
        std::size_t line_end = text.find('\n', line_begin);
        if (line_end == std::string::npos) {
            line_end = text.size();
        }

        std::size_t comment_start = findCommentStart(text, line_begin, line_end);
        if (comment_start < line_end) {
            applyStyle(styles, comment_start, line_end, kStyleComment);
        }

        std::vector<AsmToken> tokens = tokenizeAsmCode(text, line_begin, comment_start);
        for (const AsmToken& token : tokens) {
            char style = kStyleDefault;
            if (isInstructionToken(token.lower_text)) {
                style = kStyleInstruction;
            } else if (isRegisterToken(token.lower_text)) {
                style = kStyleRegister;
            } else if (isImmediateToken(token.lower_text)) {
                style = kStyleImmediate;
            } else if (isLabelToken(token.lower_text)) {
                style = kStyleLabel;
            }
            applyStyle(styles, token.start, token.end, style);
        }

        line_begin = (line_end < text.size()) ? line_end + 1 : text.size();
    }
    return styles;
}

} // namespace

class RegisterTable : public Fl_Table_Row {
public:
    RegisterTable(int x, int y, int width, int height)
        : Fl_Table_Row(x, y, width, height) {
        rows(13);
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

    void setRegisters(const lc3::RegisterView& registers) {
        registers_ = registers;
        redraw();
    }

    void fitColumns(int width) {
        int name_width = std::min(96, std::max(72, width * 42 / 100));
        col_width(0, name_width);
        col_width(1, std::max(70, width - name_width - 18));
    }

private:
    void draw_cell(TableContext context, int row, int col, int x, int y, int width, int height) override {
        switch (context) {
            case CONTEXT_STARTPAGE:
                fl_font(FL_HELVETICA, 13);
                return;
            case CONTEXT_COL_HEADER:
                drawHeaderCell(col == 0 ? "Name" : "Value", x, y, width, height);
                return;
            case CONTEXT_CELL:
                drawValueCell(row, col, x, y, width, height);
                return;
            default:
                return;
        }
    }

    static void drawHeaderCell(const char* text, int x, int y, int width, int height) {
        fl_push_clip(x, y, width, height);
        fl_draw_box(FL_THIN_UP_BOX, x, y, width, height, fl_rgb_color(232, 236, 240));
        fl_color(FL_BLACK);
        fl_font(FL_HELVETICA_BOLD, 12);
        fl_draw(text, x + 5, y, width - 10, height, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
        fl_pop_clip();
    }

    void drawValueCell(int row, int col, int x, int y, int width, int height) const {
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

    static std::string registerName(int row) {
        if (row >= 0 && row < 8) {
            return "R" + std::to_string(row);
        }
        static const std::array<const char*, 5> names = { "PC", "IR", "CC", "RUNNING", "HALTED" };
        int index = row - 8;
        return index >= 0 && index < static_cast<int>(names.size()) ? names[static_cast<std::size_t>(index)] : "";
    }

    std::string registerValue(int row) const {
        if (row >= 0 && row < 8) {
            return lc3::formatHexWord(registers_.r[row]);
        }
        switch (row) {
            case 8: return lc3::formatHexWord(registers_.pc);
            case 9: return lc3::formatHexWord(registers_.ir);
            case 10: return registers_.cc;
            case 11: return registers_.running ? "true" : "false";
            case 12: return registers_.halted ? "true" : "false";
            default: return "";
        }
    }

    lc3::RegisterView registers_;
};

class MemoryTable : public Fl_Table_Row {
public:
    MemoryTable(int x, int y, int width, int height)
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

    void setRows(const std::vector<lc3::MemoryRow>& memory_rows) {
        rows_ = memory_rows;
        Fl_Table_Row::rows(static_cast<int>(rows_.size()));
        redraw();
    }

    void fitColumns(int width) {
        int flag_width = 62;
        int address_width = 86;
        int hex_width = 78;
        int binary_width = std::max(150, width - flag_width - address_width - hex_width - 20);
        col_width(0, flag_width);
        col_width(1, address_width);
        col_width(2, hex_width);
        col_width(3, binary_width);
    }

    bool addressAtRow(int row, int& address) const {
        if (row < 0 || row >= static_cast<int>(rows_.size())) {
            return false;
        }
        address = rows_[static_cast<std::size_t>(row)].address;
        return true;
    }

private:
    void draw_cell(TableContext context, int row, int col, int x, int y, int width, int height) override {
        switch (context) {
            case CONTEXT_STARTPAGE:
                fl_font(FL_HELVETICA, 13);
                return;
            case CONTEXT_COL_HEADER:
                drawHeaderCell(columnName(col), x, y, width, height);
                return;
            case CONTEXT_CELL:
                drawValueCell(row, col, x, y, width, height);
                return;
            default:
                return;
        }
    }

    static const char* columnName(int col) {
        static const std::array<const char*, 4> names = { "Flag", "Address", "Hex", "Binary" };
        return (col >= 0 && col < static_cast<int>(names.size())) ? names[static_cast<std::size_t>(col)] : "";
    }

    static void drawHeaderCell(const char* text, int x, int y, int width, int height) {
        fl_push_clip(x, y, width, height);
        fl_draw_box(FL_THIN_UP_BOX, x, y, width, height, fl_rgb_color(232, 236, 240));
        fl_color(FL_BLACK);
        fl_font(FL_HELVETICA_BOLD, 12);
        fl_draw(text, x + 5, y, width - 10, height, FL_ALIGN_LEFT | FL_ALIGN_CENTER);
        fl_pop_clip();
    }

    void drawValueCell(int row, int col, int x, int y, int width, int height) const {
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

    static std::string valueText(const lc3::MemoryRow& memory, int col) {
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

    std::vector<lc3::MemoryRow> rows_;
};

MainWindow::MainWindow(int width, int height)
    : Fl_Double_Window(width, height, "LC-3 Assembly Studio") {
    buildUi();
    setEditorText(defaultSource());
    dirty_ = false;
    updateTitle();
    refreshSimulatorViews();
    setStatus("Ready");
    callback(onClose, this);
    resizable(editor_);
    size_range(1000, 650);
}

MainWindow::~MainWindow() {
    stopRunTimer();
    delete editor_buffer_;
    delete editor_style_buffer_;
    delete machine_buffer_;
    delete trap_input_buffer_;
    delete trap_output_buffer_;
    delete log_buffer_;
}

void MainWindow::resize(int x, int y, int width, int height) {
    Fl_Double_Window::resize(x, y, width, height);
    layoutChildren(width, height);
}

void MainWindow::buildUi() {
    menu_ = new Fl_Menu_Bar(0, 0, w(), kMenuHeight);
    menu_->add("&File/&Open...\tCtrl+O", FL_CTRL + 'o', onOpen, this);
    menu_->add("&File/Open &Demo Program", 0, onOpenDemo, this);
    menu_->add("&File/&Save\tCtrl+S", FL_CTRL + 's', onSave, this);
    menu_->add("&File/Save &As...", 0, onSaveAs, this);
    menu_->add("&File/E&xit", 0, onExit, this);
    menu_->add("&Build/&Assemble\tF7", FL_F + 7, onAssemble, this);
    menu_->add("&Build/Assemble && &Load\tCtrl+F7", FL_CTRL + (FL_F + 7), onAssembleAndLoad, this);
    menu_->add("&Run/&Run\tF5", FL_F + 5, onRun, this);
    menu_->add("&Run/&Pause\tShift+F5", FL_SHIFT + (FL_F + 5), onPause, this);
    menu_->add("&Run/&Step\tF10", FL_F + 10, onStep, this);
    menu_->add("&Run/&Reset", 0, onReset, this);
    menu_->add("&Run/Clear &Breakpoints", 0, onClearBreakpoints, this);
    menu_->add("&Build/Core Self Test", 0, onCoreSelfTest, this);

    open_button_ = new Fl_Button(kMargin, kMenuHeight + 9, 88, kButtonHeight, "Open");
    open_button_->callback(onOpen, this);
    save_button_ = new Fl_Button(kMargin + 96, kMenuHeight + 9, 88, kButtonHeight, "Save");
    save_button_->callback(onSave, this);
    assemble_button_ = new Fl_Button(kMargin + 192, kMenuHeight + 9, 116, kButtonHeight, "Assemble");
    assemble_button_->callback(onAssemble, this);
    load_button_ = new Fl_Button(kMargin + 316, kMenuHeight + 9, 96, kButtonHeight, "Load");
    load_button_->callback(onAssembleAndLoad, this);
    run_button_ = new Fl_Button(kMargin + 420, kMenuHeight + 9, 80, kButtonHeight, "Run");
    run_button_->callback(onRun, this);
    pause_button_ = new Fl_Button(kMargin + 508, kMenuHeight + 9, 80, kButtonHeight, "Pause");
    pause_button_->callback(onPause, this);
    step_button_ = new Fl_Button(kMargin + 596, kMenuHeight + 9, 80, kButtonHeight, "Step");
    step_button_->callback(onStep, this);
    reset_button_ = new Fl_Button(kMargin + 684, kMenuHeight + 9, 80, kButtonHeight, "Reset");
    reset_button_->callback(onReset, this);
    memory_jump_input_ = new Fl_Input(kMargin + 780, kMenuHeight + 9, 104, kButtonHeight, "");
    memory_jump_input_->value("x3000");
    jump_button_ = new Fl_Button(kMargin + 892, kMenuHeight + 9, 80, kButtonHeight, "Jump");
    jump_button_->callback(onJumpMemory, this);
    breakpoint_input_ = new Fl_Input(0, 0, 1, kButtonHeight, "");
    breakpoint_input_->value("x3003");
    add_breakpoint_button_ = new Fl_Button(0, 0, 1, kButtonHeight, "Add BP");
    add_breakpoint_button_->callback(onAddBreakpoint, this);
    remove_breakpoint_button_ = new Fl_Button(0, 0, 1, kButtonHeight, "Remove BP");
    remove_breakpoint_button_->callback(onRemoveBreakpoint, this);
    clear_breakpoints_button_ = new Fl_Button(0, 0, 1, kButtonHeight, "Clear BP");
    clear_breakpoints_button_->callback(onClearBreakpoints, this);

    jump_label_ = new Fl_Box(0, 0, 1, 1, "Memory");
    breakpoint_label_ = new Fl_Box(0, 0, 1, 1, "Breakpoint");
    source_label_ = new Fl_Box(0, 0, 1, 1, "ASM Source");
    machine_label_ = new Fl_Box(0, 0, 1, 1, "Machine Code");
    register_label_ = new Fl_Box(0, 0, 1, 1, "Registers");
    memory_label_ = new Fl_Box(0, 0, 1, 1, "Memory");
    trap_input_label_ = new Fl_Box(0, 0, 1, 1, "TRAP Input Buffer");
    trap_output_label_ = new Fl_Box(0, 0, 1, 1, "TRAP Output");
    trap_remaining_label_ = new Fl_Box(0, 0, 1, 1, "Remaining: 0");
    log_label_ = new Fl_Box(0, 0, 1, 1, "Log");

    jump_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    breakpoint_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    source_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    machine_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    register_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    memory_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    trap_input_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    trap_output_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    trap_remaining_label_->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    log_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    jump_label_->labelfont(FL_BOLD);
    breakpoint_label_->labelfont(FL_BOLD);
    source_label_->labelfont(FL_BOLD);
    machine_label_->labelfont(FL_BOLD);
    register_label_->labelfont(FL_BOLD);
    memory_label_->labelfont(FL_BOLD);
    trap_input_label_->labelfont(FL_BOLD);
    trap_output_label_->labelfont(FL_BOLD);
    log_label_->labelfont(FL_BOLD);

    editor_buffer_ = new Fl_Text_Buffer();
    editor_style_buffer_ = new Fl_Text_Buffer();
    machine_buffer_ = new Fl_Text_Buffer();
    trap_input_buffer_ = new Fl_Text_Buffer();
    trap_output_buffer_ = new Fl_Text_Buffer();
    log_buffer_ = new Fl_Text_Buffer();

    editor_ = new Fl_Text_Editor(0, 0, 1, 1);
    editor_->buffer(editor_buffer_);
    editor_->textfont(FL_COURIER);
    editor_->textsize(14);
    editor_->linenumber_width(48);
    editor_->linenumber_align(FL_ALIGN_RIGHT);
    editor_->linenumber_size(12);
    editor_->highlight_data(editor_style_buffer_,
                            kAsmStyleTable,
                            static_cast<int>(sizeof(kAsmStyleTable) / sizeof(kAsmStyleTable[0])),
                            kStyleDefault,
                            nullptr,
                            nullptr);
    editor_buffer_->add_modify_callback(onTextModified, this);

    machine_display_ = new Fl_Text_Display(0, 0, 1, 1);
    machine_display_->buffer(machine_buffer_);
    machine_display_->textfont(FL_COURIER);
    machine_display_->textsize(14);

    register_table_ = new RegisterTable(0, 0, 1, 1);
    memory_table_ = new MemoryTable(0, 0, 1, 1);
    memory_table_->callback(onMemoryTableEvent, this);
    memory_table_->when(FL_WHEN_RELEASE);
    memory_table_->type(Fl_Table_Row::SELECT_SINGLE);

    trap_input_editor_ = new Fl_Text_Editor(0, 0, 1, 1);
    trap_input_editor_->buffer(trap_input_buffer_);
    trap_input_editor_->textfont(FL_COURIER);
    trap_input_editor_->textsize(13);
    trap_input_buffer_->add_modify_callback(onTrapInputModified, this);

    trap_output_display_ = new Fl_Text_Display(0, 0, 1, 1);
    trap_output_display_->buffer(trap_output_buffer_);
    trap_output_display_->textfont(FL_COURIER);
    trap_output_display_->textsize(13);

    clear_trap_output_button_ = new Fl_Button(0, 0, 1, 1, "Clear");
    clear_trap_output_button_->callback(onClearTrapOutput, this);

    log_display_ = new Fl_Text_Display(0, 0, 1, 1);
    log_display_->buffer(log_buffer_);
    log_display_->textfont(FL_COURIER);
    log_display_->textsize(13);

    status_bar_ = new Fl_Box(0, 0, w(), kStatusHeight, "");
    status_bar_->box(FL_FLAT_BOX);
    status_bar_->color(fl_rgb_color(238, 238, 238));
    status_bar_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    end();
    layoutChildren(w(), h());
}

void MainWindow::layoutChildren(int width, int height) {
    menu_->resize(0, 0, width, kMenuHeight);

    int row1_y = kMenuHeight + 8;
    int row2_y = row1_y + kButtonHeight + kToolbarRowGap;
    int button_x = kMargin;
    open_button_->resize(button_x, row1_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    save_button_->resize(button_x, row1_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    assemble_button_->resize(button_x, row1_y, 100, kButtonHeight);
    button_x += 100 + kGap;
    load_button_->resize(button_x, row1_y, 78, kButtonHeight);
    button_x += 78 + kGap;
    run_button_->resize(button_x, row1_y, 64, kButtonHeight);
    button_x += 64 + kGap;
    pause_button_->resize(button_x, row1_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    step_button_->resize(button_x, row1_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    reset_button_->resize(button_x, row1_y, 72, kButtonHeight);

    button_x = kMargin;
    jump_label_->resize(button_x, row2_y, 62, kButtonHeight);
    button_x += 62 + kGap;
    int jump_input_w = std::min(116, std::max(86, width / 10));
    memory_jump_input_->resize(button_x, row2_y, jump_input_w, kButtonHeight);
    button_x += jump_input_w + kGap;
    jump_button_->resize(button_x, row2_y, 76, kButtonHeight);
    button_x += 76 + 2 * kGap;

    breakpoint_label_->resize(button_x, row2_y, 86, kButtonHeight);
    button_x += 86 + kGap;
    int breakpoint_input_w = std::min(116, std::max(86, width / 10));
    breakpoint_input_->resize(button_x, row2_y, breakpoint_input_w, kButtonHeight);
    button_x += breakpoint_input_w + kGap;
    add_breakpoint_button_->resize(button_x, row2_y, 76, kButtonHeight);
    button_x += 76 + kGap;
    remove_breakpoint_button_->resize(button_x, row2_y, 98, kButtonHeight);
    button_x += 98 + kGap;
    clear_breakpoints_button_->resize(button_x, row2_y, 84, kButtonHeight);

    int content_x = kMargin;
    int content_y = kMenuHeight + kToolbarHeight + kMargin;
    int content_w = std::max(1, width - 2 * kMargin);
    int content_h = std::max(1, height - content_y - kStatusHeight - kMargin);
    int bottom_h = std::max(104, content_h * 22 / 100);
    bottom_h = std::min(bottom_h, std::max(80, content_h / 3));
    int top_h = std::max(220, content_h * 54 / 100);
    int memory_h = content_h - top_h - bottom_h - 2 * kGap;
    if (memory_h < 120) {
        int deficit = 120 - memory_h;
        top_h = std::max(180, top_h - deficit);
        memory_h = content_h - top_h - bottom_h - 2 * kGap;
    }
    if (memory_h < 80) {
        bottom_h = std::max(80, bottom_h - (80 - memory_h));
        memory_h = std::max(1, content_h - top_h - bottom_h - 2 * kGap);
    }
    int memory_y = content_y + top_h + kGap;
    int bottom_y = memory_y + memory_h + kGap;

    int left_w = std::max(320, content_w * 48 / 100);
    int right_total_w = content_w - left_w - kGap;
    if (right_total_w < 420) {
        left_w = std::max(280, content_w - kGap - 420);
        right_total_w = std::max(1, content_w - left_w - kGap);
    }

    int register_w = std::min(std::max(190, right_total_w * 36 / 100), std::max(1, right_total_w - 220));
    int machine_w = std::max(1, right_total_w - register_w - kGap);
    if (right_total_w < 340) {
        register_w = std::max(1, right_total_w / 2);
        machine_w = std::max(1, right_total_w - register_w - kGap);
    }

    int machine_x = content_x + left_w + kGap;
    int register_x = machine_x + machine_w + kGap;

    source_label_->resize(content_x, content_y, left_w, kLabelHeight);
    editor_->resize(content_x, content_y + kLabelHeight, left_w,
                    std::max(1, top_h - kLabelHeight));

    machine_label_->resize(machine_x, content_y, machine_w, kLabelHeight);
    machine_display_->resize(machine_x, content_y + kLabelHeight, machine_w,
                             std::max(1, top_h - kLabelHeight));

    register_label_->resize(register_x, content_y, register_w, kLabelHeight);
    register_table_->resize(register_x, content_y + kLabelHeight, register_w,
                            std::max(1, top_h - kLabelHeight));
    register_table_->fitColumns(register_w);

    memory_label_->resize(content_x, memory_y, content_w, kLabelHeight);
    memory_table_->resize(content_x, memory_y + kLabelHeight, content_w,
                          std::max(1, memory_h - kLabelHeight));
    memory_table_->fitColumns(content_w);

    int available_bottom_w = std::max(1, content_w - 2 * kGap);
    int trap_input_w = std::max(220, available_bottom_w * 32 / 100);
    int trap_output_w = std::max(220, available_bottom_w * 32 / 100);
    int log_w = available_bottom_w - trap_input_w - trap_output_w;
    if (log_w < 220) {
        trap_input_w = available_bottom_w / 3;
        trap_output_w = available_bottom_w / 3;
        log_w = std::max(1, available_bottom_w - trap_input_w - trap_output_w);
    }

    int trap_input_x = content_x;
    int trap_output_x = trap_input_x + trap_input_w + kGap;
    int log_x = trap_output_x + trap_output_w + kGap;
    int bottom_body_y = bottom_y + kLabelHeight;
    int bottom_body_h = std::max(1, bottom_h - kLabelHeight);

    trap_input_label_->resize(trap_input_x, bottom_y, std::max(1, trap_input_w - 108), kLabelHeight);
    trap_remaining_label_->resize(trap_input_x + std::max(1, trap_input_w - 108), bottom_y,
                                  std::min(108, trap_input_w), kLabelHeight);
    trap_input_editor_->resize(trap_input_x, bottom_body_y, trap_input_w, bottom_body_h);

    trap_output_label_->resize(trap_output_x, bottom_y, std::max(1, trap_output_w - 72), kLabelHeight);
    clear_trap_output_button_->resize(trap_output_x + std::max(0, trap_output_w - 64),
                                      bottom_y + 2, std::min(64, trap_output_w), kLabelHeight - 4);
    trap_output_display_->resize(trap_output_x, bottom_body_y, trap_output_w, bottom_body_h);

    log_label_->resize(log_x, bottom_y, log_w, kLabelHeight);
    log_display_->resize(log_x, bottom_body_y, log_w, bottom_body_h);

    status_bar_->resize(0, height - kStatusHeight, width, kStatusHeight);
}

void MainWindow::onOpen(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->openFile();
}

void MainWindow::onSave(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->saveFile();
}

void MainWindow::onSaveAs(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->saveFileAs();
}

void MainWindow::onExit(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->requestClose();
}

void MainWindow::onAssemble(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->assembleSource();
}

void MainWindow::onAssembleAndLoad(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->assembleAndLoad();
}

void MainWindow::onRun(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->runProgram();
}

void MainWindow::onPause(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->pauseProgram();
}

void MainWindow::onStep(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->stepOnce();
}

void MainWindow::onReset(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->resetProgram();
}

void MainWindow::onJumpMemory(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->jumpMemory();
}

void MainWindow::onAddBreakpoint(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->addBreakpoint();
}

void MainWindow::onRemoveBreakpoint(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->removeBreakpoint();
}

void MainWindow::onClearBreakpoints(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->clearBreakpoints();
}

void MainWindow::onClearTrapOutput(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->clearTrapOutput();
}

void MainWindow::onCoreSelfTest(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->runCoreSelfTest();
}

void MainWindow::onOpenDemo(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->openDemoProgram();
}

void MainWindow::onMemoryTableEvent(Fl_Widget* widget, void* data) {
    auto* self = static_cast<MainWindow*>(data);
    auto* table = static_cast<MemoryTable*>(widget);
    if (table->callback_context() != Fl_Table::CONTEXT_CELL || Fl::event_clicks() == 0) {
        return;
    }

    int address = 0;
    if (table->addressAtRow(table->callback_row(), address)) {
        self->toggleBreakpointAtAddress(address);
        Fl::event_clicks(0);
    }
}

void MainWindow::onTextModified(int, int inserted, int deleted, int, const char*, void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (inserted != 0 || deleted != 0) {
        self->restyleEditor();
    }
    if (!self->programmatic_edit_ && (inserted != 0 || deleted != 0)) {
        self->dirty_ = true;
        self->updateTitle();
        self->invalidateLoadedProgram("Source changed; assemble and load before running");
    }
}

void MainWindow::onTrapInputModified(int, int inserted, int deleted, int, const char*, void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (inserted != 0 || deleted != 0) {
        self->updateTrapInputBuffer();
    }
}

void MainWindow::onClose(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->requestClose();
}

void MainWindow::onRunTimer(void* data) {
    static_cast<MainWindow*>(data)->runTimerTick();
}

void MainWindow::openFile() {
    stopRunTimer();
    if (dirty_) {
        int choice = fl_choice("The current source has unsaved changes.", "Cancel", "Discard", "Save");
        if (choice == 0) return;
        if (choice == 2 && !saveFile()) return;
    }

    Fl_Native_File_Chooser chooser;
    chooser.title("Open LC-3 assembly file");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter("LC-3 Assembly\t*.asm\nText Files\t*.txt\nAll Files\t*");

    int result = chooser.show();
    if (result == 1) return;
    if (result == -1) {
        appendLog(std::string("Open failed: ") + chooser.errmsg());
        setStatus("Open failed");
        return;
    }

    std::filesystem::path path = utf8Path(chooser.filename());
    try {
        setEditorText(readFile(path));
        current_file_ = path;
        dirty_ = false;
        machine_buffer_->text("");
        latest_machine_code_.clear();
        program_loaded_ = false;
        memory_center_ = 0x3000;
        simulator_.clearMachine();
        simulator_.clearBreakpoints();
        breakpoint_input_->value("x3003");
        refreshSimulatorViews();
        appendLog("Opened " + fileDisplayName(path));
        setStatus("Opened " + fileDisplayName(path));
        updateTitle();
    } catch (const std::exception& e) {
        fl_alert("%s", e.what());
        appendLog(e.what());
        setStatus("Open failed");
    }
}

bool MainWindow::saveFile() {
    if (current_file_.empty()) {
        return saveFileAs();
    }

    try {
        writeFile(current_file_, editorText());
        dirty_ = false;
        appendLog("Saved " + fileDisplayName(current_file_));
        setStatus("Saved " + fileDisplayName(current_file_));
        updateTitle();
        return true;
    } catch (const std::exception& e) {
        fl_alert("%s", e.what());
        appendLog(e.what());
        setStatus("Save failed");
        return false;
    }
}

bool MainWindow::saveFileAs() {
    Fl_Native_File_Chooser chooser;
    chooser.title("Save LC-3 assembly file");
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.filter("LC-3 Assembly\t*.asm\nText Files\t*.txt\nAll Files\t*");
    std::string preset_file = current_file_.empty() ? "program.asm" : fileDisplayName(current_file_);
    chooser.preset_file(preset_file.c_str());

    int result = chooser.show();
    if (result == 1) return false;
    if (result == -1) {
        appendLog(std::string("Save failed: ") + chooser.errmsg());
        setStatus("Save failed");
        return false;
    }

    current_file_ = utf8Path(chooser.filename());
    return saveFile();
}

void MainWindow::assembleSource() {
    stopRunTimer();
    std::string source = editorText();
    lc3::AssembleResult result = assembler_.assembleSource(source);
    if (!result.ok) {
        machine_buffer_->text("");
        latest_machine_code_.clear();
        appendLog("Assembly failed: " + result.error_message);
        setStatus("Assembly failed");
        return;
    }

    latest_machine_code_ = result.machine_code;
    setMachineOutput(formatMachineOutput(result.words));
    appendLog("Assembled successfully: " + std::to_string(result.words.size()) + " word(s)");
    setStatus("Assembly succeeded");
}

void MainWindow::assembleAndLoad() {
    stopRunTimer();
    std::string source = editorText();
    lc3::AssembleResult assembled = assembler_.assembleSource(source);
    if (!assembled.ok) {
        machine_buffer_->text("");
        latest_machine_code_.clear();
        program_loaded_ = false;
        simulator_.clearMachine();
        memory_center_ = 0x3000;
        appendLog("Assembly failed: " + assembled.error_message);
        setStatus("Assembly failed");
        refreshSimulatorViews();
        return;
    }

    setMachineOutput(formatMachineOutput(assembled.words));
    latest_machine_code_ = assembled.machine_code;

    lc3::OperationResult loaded = simulator_.loadMachineCode(assembled.machine_code);
    if (!loaded.ok) {
        program_loaded_ = false;
        simulator_.clearMachine();
        refreshSimulatorViews();
        appendLog("Load failed: " + loaded.message);
        setStatus("Load failed");
        return;
    }

    program_loaded_ = true;
    lc3::RegisterView registers = simulator_.registers();
    memory_center_ = registers.pc;
    refreshSimulatorViews();
    appendLog("Loaded program at " + lc3::formatHexWord(registers.loaded_start) +
              " (" + std::to_string(registers.loaded_words) + " word(s))");
    setStatus("Program loaded");
}

void MainWindow::runProgram() {
    if (!program_loaded_) {
        setStatus("Load program before running");
        appendLog("Run ignored: no program loaded");
        return;
    }

    if (simulator_.isHalted()) {
        setStatus("Reset program before running");
        appendLog("Run ignored: machine is halted");
        return;
    }

    lc3::OperationResult result = simulator_.setRunning(true);
    if (!result.ok) {
        setStatus("Run failed");
        appendLog("Run failed: " + result.message);
        refreshSimulatorViews();
        return;
    }

    run_timer_active_ = true;
    Fl::remove_timeout(onRunTimer, this);
    Fl::add_timeout(0.0, onRunTimer, this);
    refreshSimulatorViews();
    appendLog("Run started");
    setStatus("Running");
}

void MainWindow::pauseProgram() {
    if (!run_timer_active_ && !simulator_.isRunning()) {
        setStatus("Already paused");
        return;
    }

    stopRunTimer();
    refreshSimulatorViews();
    appendLog("Run paused");
    setStatus("Paused");
}

void MainWindow::stopRunTimer() {
    if (run_timer_active_) {
        Fl::remove_timeout(onRunTimer, this);
    }
    run_timer_active_ = false;
    if (simulator_.isRunning()) {
        (void)simulator_.setRunning(false);
    }
}

void MainWindow::runTimerTick() {
    if (!run_timer_active_) {
        return;
    }

    if (!program_loaded_ || !simulator_.isRunning()) {
        run_timer_active_ = false;
        refreshSimulatorViews();
        return;
    }

    std::string stop_message;
    bool should_continue = true;
    for (int i = 0; i < kRunStepsPerTick; i++) {
        lc3::RunStepResult result = simulator_.stepForRun();
        if (!result.ok) {
            stop_message = "Run failed: " + result.message;
            should_continue = false;
            break;
        }
        if (result.stopped) {
            stop_message = result.message;
            should_continue = false;
            break;
        }
    }

    lc3::RegisterView registers = simulator_.registers();
    memory_center_ = registers.pc;
    refreshSimulatorViews();

    if (!should_continue) {
        run_timer_active_ = false;
        appendLog(stop_message);
        setStatus(stop_message);
        return;
    }

    if (simulator_.isRunning()) {
        Fl::repeat_timeout(kRunTickSeconds, onRunTimer, this);
    } else {
        run_timer_active_ = false;
        setStatus("Paused");
    }
}

void MainWindow::stepOnce() {
    stopRunTimer();
    if (!program_loaded_) {
        setStatus("Load program before stepping");
        appendLog("Step ignored: no program loaded");
        return;
    }

    lc3::OperationResult result = simulator_.stepOnce();
    lc3::RegisterView registers = simulator_.registers();
    memory_center_ = registers.pc;
    refreshSimulatorViews();

    if (!result.ok) {
        appendLog("Step failed: " + result.message);
        setStatus("Step failed");
        return;
    }

    if (registers.halted) {
        appendLog("Step: machine halted at " + lc3::formatHexWord(registers.pc));
        setStatus("Machine halted");
    } else {
        setStatus("Step executed; PC=" + lc3::formatHexWord(registers.pc));
    }
}

void MainWindow::resetProgram() {
    stopRunTimer();
    lc3::OperationResult result = simulator_.resetProgram();
    if (!result.ok) {
        appendLog("Reset failed: " + result.message);
        setStatus("Reset failed");
        return;
    }

    lc3::RegisterView registers = simulator_.registers();
    program_loaded_ = registers.loaded_words > 0;
    memory_center_ = registers.pc;
    refreshSimulatorViews();
    appendLog(program_loaded_ ? "Program reset" : "Machine cleared");
    setStatus(program_loaded_ ? "Program reset" : "Machine cleared");
}

void MainWindow::jumpMemory() {
    int address = 0;
    if (!lc3::parseAddress(memory_jump_input_->value(), address)) {
        setStatus("Invalid memory address");
        appendLog(std::string("Invalid memory address: ") + memory_jump_input_->value());
        return;
    }

    memory_center_ = address & 0xFFFF;
    refreshMemoryView();
    setStatus("Memory centered at " + lc3::formatHexWord(memory_center_));
}

void MainWindow::addBreakpoint() {
    int address = 0;
    if (!lc3::parseAddress(breakpoint_input_->value(), address)) {
        setStatus("Invalid breakpoint address");
        appendLog(std::string("Invalid breakpoint address: ") + breakpoint_input_->value());
        return;
    }

    address &= 0xFFFF;
    simulator_.setBreakpoint(address, true);
    breakpoint_input_->value(lc3::formatHexWord(address).c_str());
    refreshMemoryView();
    appendLog("Breakpoint set at " + lc3::formatHexWord(address));
    setStatus("Breakpoint set at " + lc3::formatHexWord(address));
}

void MainWindow::removeBreakpoint() {
    int address = 0;
    if (!lc3::parseAddress(breakpoint_input_->value(), address)) {
        setStatus("Invalid breakpoint address");
        appendLog(std::string("Invalid breakpoint address: ") + breakpoint_input_->value());
        return;
    }

    address &= 0xFFFF;
    simulator_.setBreakpoint(address, false);
    breakpoint_input_->value(lc3::formatHexWord(address).c_str());
    refreshMemoryView();
    appendLog("Breakpoint removed at " + lc3::formatHexWord(address));
    setStatus("Breakpoint removed at " + lc3::formatHexWord(address));
}

void MainWindow::clearBreakpoints() {
    simulator_.clearBreakpoints();
    refreshMemoryView();
    appendLog("All breakpoints cleared");
    setStatus("All breakpoints cleared");
}

void MainWindow::toggleBreakpointAtAddress(int address) {
    address &= 0xFFFF;
    bool enabled = !simulator_.hasBreakpoint(address);
    simulator_.setBreakpoint(address, enabled);
    breakpoint_input_->value(lc3::formatHexWord(address).c_str());
    refreshMemoryView();
    appendLog(std::string(enabled ? "Breakpoint set at " : "Breakpoint removed at ") +
              lc3::formatHexWord(address));
    setStatus(std::string(enabled ? "Breakpoint set at " : "Breakpoint removed at ") +
              lc3::formatHexWord(address));
}

void MainWindow::clearTrapOutput() {
    simulator_.clearTrapOutputBuffer();
    refreshTrapViews();
    appendLog("TRAP output cleared");
    setStatus("TRAP output cleared");
}

void MainWindow::runCoreSelfTest() {
    lc3::AssembleResult result = assembler_.assembleSource(defaultSource());
    if (!result.ok) {
        appendLog("Core self test assembly failed: " + result.error_message);
        setStatus("Core self test failed");
        return;
    }

    lc3::SimulatorService simulator;
    lc3::OperationResult load = simulator.loadMachineCode(result.machine_code);
    if (!load.ok) {
        appendLog("Core self test load failed: " + load.message);
        setStatus("Core self test failed");
        return;
    }

    lc3::OperationResult step = simulator.stepOnce();
    lc3::RegisterView registers = simulator.registers();
    if (!step.ok) {
        appendLog("Core self test step failed: " + step.message);
        setStatus("Core self test failed");
        return;
    }

    appendLog("Core self test OK: PC=" + lc3::formatHexWord(registers.pc) +
              " R0=" + lc3::formatHexWord(registers.r[0]) +
              " CC=" + registers.cc);
    setStatus("Core self test OK");
}

void MainWindow::openDemoProgram() {
    stopRunTimer();
    if (dirty_) {
        int choice = fl_choice("The current source has unsaved changes.", "Cancel", "Discard", "Save");
        if (choice == 0) return;
        if (choice == 2 && !saveFile()) return;
    }

    setEditorText(defaultSource());
    current_file_.clear();
    dirty_ = false;
    machine_buffer_->text("");
    latest_machine_code_.clear();
    program_loaded_ = false;
    memory_center_ = 0x3000;
    memory_jump_input_->value("x3000");
    breakpoint_input_->value("x3003");
    simulator_.clearMachine();
    simulator_.clearBreakpoints();
    refreshSimulatorViews();
    appendLog("Loaded demo program");
    setStatus("Demo program loaded; x3003 is ready for breakpoint demo");
    updateTitle();
}

void MainWindow::requestClose() {
    stopRunTimer();
    if (dirty_) {
        int choice = fl_choice("The current source has unsaved changes.", "Cancel", "Discard", "Save");
        if (choice == 0) return;
        if (choice == 2 && !saveFile()) return;
    }
    hide();
}

void MainWindow::setEditorText(const std::string& text) {
    programmatic_edit_ = true;
    editor_buffer_->text(text.c_str());
    restyleEditor();
    programmatic_edit_ = false;
}

std::string MainWindow::editorText() const {
    char* text = editor_buffer_->text();
    std::string result = text ? text : "";
    std::free(text);
    return result;
}

void MainWindow::restyleEditor() {
    if (!editor_buffer_ || !editor_style_buffer_) {
        return;
    }
    std::string styles = buildAsmStyleText(editorText());
    editor_style_buffer_->text(styles.c_str());
    editor_->redisplay_range(0, editor_buffer_->length());
}

std::string MainWindow::trapInputText() const {
    char* text = trap_input_buffer_->text();
    std::string result = text ? text : "";
    std::free(text);
    return result;
}

void MainWindow::setMachineOutput(const std::string& text) {
    machine_buffer_->text(text.c_str());
}

void MainWindow::appendLog(const std::string& message) {
    std::string line = message + "\n";
    log_buffer_->append(line.c_str());
    log_display_->insert_position(log_buffer_->length());
    log_display_->show_insert_position();
}

void MainWindow::setStatus(const std::string& message) {
    status_bar_->copy_label(("  " + message).c_str());
}

void MainWindow::invalidateLoadedProgram(const std::string& message) {
    lc3::RegisterView registers = simulator_.registers();
    bool had_loaded_program = program_loaded_ || registers.running || registers.loaded_words > 0;
    bool had_machine_output = !latest_machine_code_.empty() ||
                              (machine_buffer_ && machine_buffer_->length() > 0);

    if (!had_loaded_program && !had_machine_output) {
        updateControlStates(registers);
        return;
    }

    stopRunTimer();
    program_loaded_ = false;
    latest_machine_code_.clear();
    machine_buffer_->text("");
    simulator_.clearMachine();
    memory_center_ = 0x3000;
    memory_jump_input_->value("x3000");
    refreshSimulatorViews();
    appendLog(message);
    setStatus(message);
}

void MainWindow::updateTrapInputBuffer() {
    simulator_.setTrapInputBuffer(trapInputText());
    refreshTrapViews();
}

void MainWindow::refreshSimulatorViews() {
    lc3::RegisterView registers = simulator_.registers();
    refreshRegisterView(registers);
    refreshMemoryView();
    refreshTrapViews();
    updateControlStates(registers);
}

void MainWindow::refreshRegisterView(const lc3::RegisterView& registers) {
    register_table_->setRegisters(registers);
}

void MainWindow::refreshMemoryView() {
    memory_table_->setRows(simulator_.memoryWindow(memory_center_, kMemoryRowsBeforePc, kMemoryRowsAfterPc));
}

void MainWindow::refreshTrapViews() {
    std::string output = simulator_.trapOutputBuffer();
    trap_output_buffer_->text(output.c_str());
    trap_output_display_->insert_position(trap_output_buffer_->length());
    trap_output_display_->show_insert_position();

    std::string remaining = simulator_.trapInputRemainder();
    std::string label = "Remaining: " + std::to_string(remaining.size());
    trap_remaining_label_->copy_label(label.c_str());
}

void MainWindow::updateControlStates(const lc3::RegisterView& registers) {
    bool can_execute = program_loaded_ && !registers.running && !registers.halted;
    bool can_run = program_loaded_ && !registers.running && !registers.halted;

    if (run_button_) {
        can_run ? run_button_->activate() : run_button_->deactivate();
    }
    if (pause_button_) {
        registers.running ? pause_button_->activate() : pause_button_->deactivate();
    }
    if (step_button_) {
        can_execute ? step_button_->activate() : step_button_->deactivate();
    }
}

void MainWindow::updateTitle() {
    std::string title = "LC-3 Assembly Studio";
    if (!current_file_.empty()) {
        title += " - " + fileDisplayName(current_file_);
    }
    if (dirty_) {
        title += " *";
    }
    copy_label(title.c_str());
}

std::string MainWindow::defaultSource() {
    return ".orig x3000\n"
           "        and r0, r0, #0\n"
           "        add r0, r0, #5\n"
           "        add r1, r0, #3\n"
           "        lea r0, msg\n"
           "        puts\n"
           "        halt\n"
           "msg     .stringz \"LC-3 GUI OK\\n\"\n"
           ".end\n";
}

std::string MainWindow::fileDisplayName(const std::filesystem::path& path) {
    if (path.empty()) {
        return "Untitled";
    }
    auto name = path.filename().u8string();
    return std::string(name.begin(), name.end());
}

std::string MainWindow::formatMachineOutput(const std::vector<std::string>& words) {
    std::ostringstream output;
    output << "Address  Binary            Hex\n";
    output << "-------  ----------------  -----\n";

    int address = 0x3000;
    for (const std::string& word : words) {
        int value = parseBinaryWord(word);
        output << lc3::formatHexWord(address) << "    "
               << word << "  "
               << lc3::formatHexWord(value) << "\n";
        address++;
    }
    return output.str();
}

int MainWindow::parseBinaryWord(const std::string& word) {
    int value = 0;
    for (char c : word) {
        value <<= 1;
        if (c == '1') {
            value |= 1;
        }
    }
    return value & 0xFFFF;
}
