#include "main_window.h"

#include <FL/Enumerations.H>
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
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

constexpr int kMenuHeight = 28;
constexpr int kToolbarHeight = 48;
constexpr int kStatusHeight = 26;
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kLabelHeight = 22;
constexpr int kButtonHeight = 30;
constexpr int kMemoryRowsBeforePc = 8;
constexpr int kMemoryRowsAfterPc = 32;

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
    size_range(900, 560);
}

MainWindow::~MainWindow() {
    delete editor_buffer_;
    delete machine_buffer_;
    delete log_buffer_;
}

void MainWindow::resize(int x, int y, int width, int height) {
    Fl_Double_Window::resize(x, y, width, height);
    layoutChildren(width, height);
}

void MainWindow::buildUi() {
    menu_ = new Fl_Menu_Bar(0, 0, w(), kMenuHeight);
    menu_->add("&File/&Open...\tCtrl+O", FL_CTRL + 'o', onOpen, this);
    menu_->add("&File/&Save\tCtrl+S", FL_CTRL + 's', onSave, this);
    menu_->add("&File/Save &As...", 0, onSaveAs, this);
    menu_->add("&File/E&xit", 0, onExit, this);
    menu_->add("&Build/&Assemble\tF7", FL_F + 7, onAssemble, this);
    menu_->add("&Build/Assemble && &Load\tCtrl+F7", FL_CTRL + (FL_F + 7), onAssembleAndLoad, this);
    menu_->add("&Run/&Step\tF10", FL_F + 10, onStep, this);
    menu_->add("&Run/&Reset", 0, onReset, this);
    menu_->add("&Build/Core Self Test", 0, onCoreSelfTest, this);

    open_button_ = new Fl_Button(kMargin, kMenuHeight + 9, 88, kButtonHeight, "Open");
    open_button_->callback(onOpen, this);
    save_button_ = new Fl_Button(kMargin + 96, kMenuHeight + 9, 88, kButtonHeight, "Save");
    save_button_->callback(onSave, this);
    assemble_button_ = new Fl_Button(kMargin + 192, kMenuHeight + 9, 116, kButtonHeight, "Assemble");
    assemble_button_->callback(onAssemble, this);
    load_button_ = new Fl_Button(kMargin + 316, kMenuHeight + 9, 96, kButtonHeight, "Load");
    load_button_->callback(onAssembleAndLoad, this);
    step_button_ = new Fl_Button(kMargin + 420, kMenuHeight + 9, 80, kButtonHeight, "Step");
    step_button_->callback(onStep, this);
    reset_button_ = new Fl_Button(kMargin + 508, kMenuHeight + 9, 80, kButtonHeight, "Reset");
    reset_button_->callback(onReset, this);
    memory_jump_input_ = new Fl_Input(kMargin + 604, kMenuHeight + 9, 104, kButtonHeight, "");
    memory_jump_input_->value("x3000");
    jump_button_ = new Fl_Button(kMargin + 716, kMenuHeight + 9, 80, kButtonHeight, "Jump");
    jump_button_->callback(onJumpMemory, this);

    source_label_ = new Fl_Box(0, 0, 1, 1, "ASM Source");
    machine_label_ = new Fl_Box(0, 0, 1, 1, "Machine Code");
    register_label_ = new Fl_Box(0, 0, 1, 1, "Registers");
    memory_label_ = new Fl_Box(0, 0, 1, 1, "Memory");
    log_label_ = new Fl_Box(0, 0, 1, 1, "Log");

    source_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    machine_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    register_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    memory_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    log_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    source_label_->labelfont(FL_BOLD);
    machine_label_->labelfont(FL_BOLD);
    register_label_->labelfont(FL_BOLD);
    memory_label_->labelfont(FL_BOLD);
    log_label_->labelfont(FL_BOLD);

    editor_buffer_ = new Fl_Text_Buffer();
    machine_buffer_ = new Fl_Text_Buffer();
    log_buffer_ = new Fl_Text_Buffer();

    editor_ = new Fl_Text_Editor(0, 0, 1, 1);
    editor_->buffer(editor_buffer_);
    editor_->textfont(FL_COURIER);
    editor_->textsize(14);
    editor_->linenumber_width(48);
    editor_->linenumber_align(FL_ALIGN_RIGHT);
    editor_->linenumber_size(12);
    editor_buffer_->add_modify_callback(onTextModified, this);

    machine_display_ = new Fl_Text_Display(0, 0, 1, 1);
    machine_display_->buffer(machine_buffer_);
    machine_display_->textfont(FL_COURIER);
    machine_display_->textsize(14);

    register_table_ = new RegisterTable(0, 0, 1, 1);
    memory_table_ = new MemoryTable(0, 0, 1, 1);

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

    int button_y = kMenuHeight + 9;
    int button_x = kMargin;
    open_button_->resize(button_x, button_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    save_button_->resize(button_x, button_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    assemble_button_->resize(button_x, button_y, 100, kButtonHeight);
    button_x += 100 + kGap;
    load_button_->resize(button_x, button_y, 78, kButtonHeight);
    button_x += 78 + kGap;
    step_button_->resize(button_x, button_y, 72, kButtonHeight);
    button_x += 72 + kGap;
    reset_button_->resize(button_x, button_y, 72, kButtonHeight);
    button_x += 72 + 2 * kGap;
    int jump_input_w = std::min(110, std::max(76, width - button_x - 84 - kMargin));
    memory_jump_input_->resize(button_x, button_y, jump_input_w, kButtonHeight);
    button_x += jump_input_w + kGap;
    jump_button_->resize(button_x, button_y, 76, kButtonHeight);

    int content_x = kMargin;
    int content_y = kMenuHeight + kToolbarHeight + kMargin;
    int content_w = std::max(1, width - 2 * kMargin);
    int content_h = std::max(1, height - content_y - kStatusHeight - kMargin);
    int top_h = std::max(250, content_h * 58 / 100);
    top_h = std::min(top_h, std::max(1, content_h - kGap - 160));
    if (top_h < 180) top_h = std::max(1, content_h * 60 / 100);
    int bottom_y = content_y + top_h + kGap;
    int bottom_h = std::max(1, content_y + content_h - bottom_y);

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

    int memory_w = std::max(1, content_w * 70 / 100);
    int log_w = std::max(1, content_w - memory_w - kGap);
    if (log_w < 220 && content_w > 500) {
        log_w = 220;
        memory_w = std::max(1, content_w - log_w - kGap);
    }
    int log_x = content_x + memory_w + kGap;

    memory_label_->resize(content_x, bottom_y, memory_w, kLabelHeight);
    memory_table_->resize(content_x, bottom_y + kLabelHeight, memory_w,
                          std::max(1, bottom_h - kLabelHeight));
    memory_table_->fitColumns(memory_w);

    log_label_->resize(log_x, bottom_y, log_w, kLabelHeight);
    log_display_->resize(log_x, bottom_y + kLabelHeight, log_w,
                         std::max(1, bottom_h - kLabelHeight));

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

void MainWindow::onStep(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->stepOnce();
}

void MainWindow::onReset(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->resetProgram();
}

void MainWindow::onJumpMemory(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->jumpMemory();
}

void MainWindow::onCoreSelfTest(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->runCoreSelfTest();
}

void MainWindow::onTextModified(int, int inserted, int deleted, int, const char*, void* data) {
    auto* self = static_cast<MainWindow*>(data);
    if (!self->programmatic_edit_ && (inserted != 0 || deleted != 0)) {
        self->dirty_ = true;
        self->updateTitle();
    }
}

void MainWindow::onClose(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->requestClose();
}

void MainWindow::openFile() {
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

void MainWindow::stepOnce() {
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

void MainWindow::requestClose() {
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
    programmatic_edit_ = false;
}

std::string MainWindow::editorText() const {
    char* text = editor_buffer_->text();
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

void MainWindow::refreshSimulatorViews() {
    lc3::RegisterView registers = simulator_.registers();
    refreshRegisterView(registers);
    refreshMemoryView();
}

void MainWindow::refreshRegisterView(const lc3::RegisterView& registers) {
    register_table_->setRegisters(registers);
}

void MainWindow::refreshMemoryView() {
    memory_table_->setRows(simulator_.memoryWindow(memory_center_, kMemoryRowsBeforePc, kMemoryRowsAfterPc));
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
