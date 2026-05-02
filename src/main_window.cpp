#include "main_window.h"

#include <FL/Enumerations.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/fl_ask.H>

#include <algorithm>
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

MainWindow::MainWindow(int width, int height)
    : Fl_Double_Window(width, height, "LC-3 Assembly Studio") {
    buildUi();
    setEditorText(defaultSource());
    dirty_ = false;
    updateTitle();
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
    menu_->add("&Build/Core Self Test", 0, onCoreSelfTest, this);

    open_button_ = new Fl_Button(kMargin, kMenuHeight + 9, 88, kButtonHeight, "Open");
    open_button_->callback(onOpen, this);
    save_button_ = new Fl_Button(kMargin + 96, kMenuHeight + 9, 88, kButtonHeight, "Save");
    save_button_->callback(onSave, this);
    assemble_button_ = new Fl_Button(kMargin + 192, kMenuHeight + 9, 116, kButtonHeight, "Assemble");
    assemble_button_->callback(onAssemble, this);

    source_label_ = new Fl_Box(0, 0, 1, 1, "ASM Source");
    machine_label_ = new Fl_Box(0, 0, 1, 1, "Machine Code");
    log_label_ = new Fl_Box(0, 0, 1, 1, "Log");

    source_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    machine_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    log_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    source_label_->labelfont(FL_BOLD);
    machine_label_->labelfont(FL_BOLD);
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
    open_button_->resize(kMargin, button_y, 88, kButtonHeight);
    save_button_->resize(kMargin + 96, button_y, 88, kButtonHeight);
    assemble_button_->resize(kMargin + 192, button_y, 116, kButtonHeight);

    int content_x = kMargin;
    int content_y = kMenuHeight + kToolbarHeight + kMargin;
    int content_w = std::max(1, width - 2 * kMargin);
    int content_h = std::max(1, height - content_y - kStatusHeight - kMargin);
    int left_w = std::max(360, (content_w - kGap) * 58 / 100);
    left_w = std::min(left_w, content_w - kGap - 280);
    if (left_w < 280) left_w = std::max(1, content_w - kGap - 280);
    int right_x = content_x + left_w + kGap;
    int right_w = std::max(1, content_w - left_w - kGap);

    source_label_->resize(content_x, content_y, left_w, kLabelHeight);
    editor_->resize(content_x, content_y + kLabelHeight, left_w,
                    std::max(1, content_h - kLabelHeight));

    int right_body_h = std::max(1, content_h - 2 * kLabelHeight - kGap);
    int machine_h = std::max(150, right_body_h * 58 / 100);
    machine_h = std::min(machine_h, right_body_h - 100);
    if (machine_h < 80) machine_h = std::max(1, right_body_h / 2);

    machine_label_->resize(right_x, content_y, right_w, kLabelHeight);
    machine_display_->resize(right_x, content_y + kLabelHeight, right_w, machine_h);

    int log_y = content_y + kLabelHeight + machine_h + kGap;
    log_label_->resize(right_x, log_y, right_w, kLabelHeight);
    log_display_->resize(right_x, log_y + kLabelHeight, right_w,
                         std::max(1, content_y + content_h - (log_y + kLabelHeight)));

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
        appendLog("Assembly failed: " + result.error_message);
        setStatus("Assembly failed");
        return;
    }

    setMachineOutput(formatMachineOutput(result.words));
    appendLog("Assembled successfully: " + std::to_string(result.words.size()) + " word(s)");
    setStatus("Assembly succeeded");
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
