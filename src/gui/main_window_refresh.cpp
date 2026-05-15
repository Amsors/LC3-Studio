#include "main_window_internal.h"

using namespace gui::main_window_detail;

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
    std::string styles = ui::buildAsmStyleText(editorText());
    editor_style_buffer_->text(styles.c_str());
    editor_->redisplay_range(0, editor_buffer_->length());
}

void MainWindow::focusEditorLine(int source_line) {
    if (!editor_buffer_ || !editor_ || source_line <= 0) {
        return;
    }

    int position = 0;
    int line = 1;
    const int length = editor_buffer_->length();
    while (line < source_line && position < length) {
        if (editor_buffer_->byte_at(position) == '\n') {
            line++;
        }
        position++;
    }

    const int line_start = position;
    const int line_end = editor_buffer_->line_end(line_start);
    editor_buffer_->select(line_start, line_end);
    editor_->insert_position(line_start);
    editor_->show_insert_position();
    editor_->take_focus();
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
    latest_word_sources_.clear();
    machine_buffer_->text("");
    simulator_.clearMachine();
    state_modified_ = false;
    state_modified_label_->copy_label("");
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

void MainWindow::refreshMemoryView(bool reset_scroll) {
    std::vector<lc3::MemoryRow> rows =
        simulator_.memoryWindow(memory_center_, kMemoryRowsBeforePc, kMemoryRowsAfterPc);
    applyMemorySources(rows);
    memory_table_->setRows(rows);
    if (reset_scroll || reset_memory_scroll_on_next_refresh_) {
        memory_table_->scrollAddressNearUpperMiddle(memory_center_);
        reset_memory_scroll_on_next_refresh_ = false;
    }
}

void MainWindow::autoScrollMemoryToPc(const lc3::RegisterView& registers) {
    if (!auto_memory_scroll_enabled_) {
        return;
    }
    memory_center_ = registers.pc & 0xFFFF;
    if (memory_jump_input_) {
        memory_jump_input_->value(lc3::formatHexWord(memory_center_).c_str());
    }
    reset_memory_scroll_on_next_refresh_ = true;
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
    std::string title = "LC-3 Studio";
    if (!current_file_.empty()) {
        title += " - " + fileDisplayName(current_file_);
    } else if (!current_example_title_.empty()) {
        title += " - " + current_example_title_;
    }
    if (dirty_) {
        title += " *";
    }
    copy_label(title.c_str());
}

void MainWindow::applyMemorySources(std::vector<lc3::MemoryRow>& rows) const {
    if (!program_loaded_ || latest_word_sources_.empty()) {
        return;
    }

    lc3::RegisterView registers = simulator_.registers();
    int loaded_start = registers.loaded_start & 0xFFFF;
    for (lc3::MemoryRow& row : rows) {
        int offset = (row.address & 0xFFFF) - loaded_start;
        if (offset >= 0 && offset < static_cast<int>(latest_word_sources_.size())) {
            row.source = latest_word_sources_[static_cast<std::size_t>(offset)];
        }
    }
}

std::string MainWindow::defaultSource() {
    const embedded_examples::AssemblyExample* example = embedded_examples::exampleAt(0);
    if (example && example->source) {
        return example->source;
    }
    return ".orig x3000\n"
           "        halt\n"
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
