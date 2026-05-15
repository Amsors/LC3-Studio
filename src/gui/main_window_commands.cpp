#include "main_window_internal.h"

using namespace gui::main_window_detail;

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

    std::filesystem::path path = ui::utf8Path(chooser.filename());
    try {
        setEditorText(ui::readFile(path));
        current_file_ = path;
        current_example_title_.clear();
        dirty_ = false;
        machine_buffer_->text("");
        latest_machine_code_.clear();
        latest_word_sources_.clear();
        latest_word_source_lines_.clear();
        program_loaded_ = false;
        state_modified_ = false;
        state_modified_label_->copy_label("");
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
        ui::writeFile(current_file_, editorText());
        dirty_ = false;
        current_example_title_.clear();
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

    current_file_ = ui::utf8Path(chooser.filename());
    current_example_title_.clear();
    return saveFile();
}

void MainWindow::assembleSource() {
    stopRunTimer();
    std::string source = editorText();
    lc3::AssembleResult result = assembler_.assembleSource(source);
    if (!result.ok) {
        machine_buffer_->text("");
        latest_machine_code_.clear();
        latest_word_sources_.clear();
        latest_word_source_lines_.clear();
        refreshEditorBreakpointMarkers();
        focusEditorLine(result.error_line);
        appendLog("Assembly failed: " + result.error_message);
        setStatus(result.error_line > 0
                      ? "Assembly failed at line " + std::to_string(result.error_line)
                      : "Assembly failed");
        return;
    }

    latest_machine_code_ = result.machine_code;
    latest_word_sources_ = result.word_sources;
    latest_word_source_lines_ = result.word_source_lines;
    refreshEditorBreakpointMarkers();
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
        latest_word_sources_.clear();
        latest_word_source_lines_.clear();
        program_loaded_ = false;
        simulator_.clearMachine();
        state_modified_ = false;
        state_modified_label_->copy_label("");
        memory_center_ = 0x3000;
        focusEditorLine(assembled.error_line);
        appendLog("Assembly failed: " + assembled.error_message);
        setStatus(assembled.error_line > 0
                      ? "Assembly failed at line " + std::to_string(assembled.error_line)
                      : "Assembly failed");
        refreshSimulatorViews();
        return;
    }

    setMachineOutput(formatMachineOutput(assembled.words));
    latest_machine_code_ = assembled.machine_code;
    latest_word_sources_ = assembled.word_sources;
    latest_word_source_lines_ = assembled.word_source_lines;

    lc3::OperationResult loaded = simulator_.loadMachineCode(assembled.machine_code);
    if (!loaded.ok) {
        program_loaded_ = false;
        simulator_.clearMachine();
        state_modified_ = false;
        state_modified_label_->copy_label("");
        refreshSimulatorViews();
        appendLog("Load failed: " + loaded.message);
        setStatus("Load failed");
        return;
    }

    program_loaded_ = true;
    state_modified_ = false;
    state_modified_label_->copy_label("");
    lc3::RegisterView registers = simulator_.registers();
    autoScrollMemoryToPc(registers);
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
    appendLog("Run started at " + std::to_string(run_rate_limit_) + " instructions/s");
    setStatus("Running at " + std::to_string(run_rate_limit_) + " instructions/s");
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
    int steps_this_tick = runStepsPerTick();
    for (int i = 0; i < steps_this_tick; i++) {
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
    autoScrollMemoryToPc(registers);
    refreshSimulatorViews();

    if (!should_continue) {
        run_timer_active_ = false;
        appendLog(stop_message);
        setStatus(stop_message);
        return;
    }

    if (simulator_.isRunning()) {
        Fl::repeat_timeout(runTickSeconds(), onRunTimer, this);
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
    if (result.ok) {
        autoScrollMemoryToPc(registers);
    }
    refreshSimulatorViews();

    if (!result.ok) {
        appendLog("Step failed: " + result.message);
        setStatus("Step failed");
        return;
    }

    if (registers.halted) {
        appendLog("Step: machine halted at " + lc3::formatHexWord(registers.pc) +
                  "; steps=" + std::to_string(registers.executed_instructions));
        setStatus("Machine halted; steps=" + std::to_string(registers.executed_instructions));
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
    state_modified_ = false;
    state_modified_label_->copy_label("");
    autoScrollMemoryToPc(registers);
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

void MainWindow::jumpMemoryToPc() {
    lc3::RegisterView registers = simulator_.registers();
    memory_center_ = registers.pc & 0xFFFF;
    memory_jump_input_->value(lc3::formatHexWord(memory_center_).c_str());
    refreshMemoryView(true);
    setStatus("Memory centered at PC " + lc3::formatHexWord(memory_center_));
}

void MainWindow::setAutoMemoryScroll(bool enabled) {
    auto_memory_scroll_enabled_ = enabled;
    if (auto_memory_scroll_check_ && auto_memory_scroll_check_->value() != (enabled ? 1 : 0)) {
        auto_memory_scroll_check_->value(enabled ? 1 : 0);
    }

    if (enabled) {
        autoScrollMemoryToPc(simulator_.registers());
        refreshMemoryView();
        setStatus("Auto PC memory scroll enabled");
    } else {
        setStatus("Auto PC memory scroll disabled");
    }
}

void MainWindow::setRunRateLimit(int instructions_per_second, bool log_change) {
    run_rate_limit_ = std::clamp(instructions_per_second, kMinRunRateLimit, kMaxRunRateLimit);
    updateRunRateControls();

    std::string message = "Run rate limit: " + std::to_string(run_rate_limit_) +
                          " instructions/s";
    if (log_change) {
        appendLog(message);
    }
    setStatus(message);
}

void MainWindow::updateRunRateControls() {
    if (run_rate_input_) {
        std::string text = std::to_string(run_rate_limit_);
        run_rate_input_->value(text.c_str());
    }
    if (run_rate_slider_) {
        run_rate_slider_->value(runRateToSliderValue(run_rate_limit_));
    }
    if (run_rate_unit_label_) {
        run_rate_unit_label_->copy_label("instructions/s");
    }
}

int MainWindow::runStepsPerTick() const {
    double desired_steps = static_cast<double>(run_rate_limit_) * kRunTimerBaseSeconds;
    return std::max(1, static_cast<int>(std::lround(desired_steps)));
}

double MainWindow::runTickSeconds() const {
    return std::max(0.001, static_cast<double>(runStepsPerTick()) /
                               static_cast<double>(std::max(1, run_rate_limit_)));
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

void MainWindow::toggleBreakpointAtSourceLine(int source_line) {
    int address = 0;
    if (!addressForSourceLine(source_line, address)) {
        if (latest_word_source_lines_.empty()) {
            setStatus("Assemble source before setting source breakpoints");
            appendLog("Source breakpoint ignored: assemble source first");
        } else {
            setStatus("No machine word for source line " + std::to_string(source_line));
            appendLog("Source breakpoint ignored: line " + std::to_string(source_line) +
                      " does not emit a machine word");
        }
        return;
    }

    bool enabled = !simulator_.hasBreakpoint(address);
    simulator_.setBreakpoint(address, enabled);
    breakpoint_input_->value(lc3::formatHexWord(address).c_str());
    memory_center_ = address;
    memory_jump_input_->value(lc3::formatHexWord(address).c_str());
    refreshMemoryView(true);

    std::string action = enabled ? "Breakpoint set" : "Breakpoint removed";
    std::string message = action + " at " + lc3::formatHexWord(address) +
                          " from source line " + std::to_string(source_line);
    appendLog(message);
    setStatus(message);
}

bool MainWindow::addressForSourceLine(int source_line, int& address) const {
    if (source_line <= 0 || latest_word_source_lines_.empty()) {
        return false;
    }

    int loaded_start = program_loaded_ ? simulator_.registers().loaded_start : 0x3000;
    for (std::size_t i = 0; i < latest_word_source_lines_.size(); i++) {
        if (latest_word_source_lines_[i] == source_line) {
            address = (loaded_start + static_cast<int>(i)) & 0xFFFF;
            return true;
        }
    }
    return false;
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

void MainWindow::openExampleProgram(const embedded_examples::AssemblyExample& example) {
    stopRunTimer();
    if (dirty_) {
        int choice = fl_choice("The current source has unsaved changes.", "Cancel", "Discard", "Save");
        if (choice == 0) return;
        if (choice == 2 && !saveFile()) return;
    }

    std::string title = example.title && *example.title ? example.title : exampleMenuTitle(example);
    setEditorText(example.source ? example.source : "");
    current_file_.clear();
    current_example_title_ = title;
    dirty_ = false;
    machine_buffer_->text("");
    latest_machine_code_.clear();
    latest_word_sources_.clear();
    latest_word_source_lines_.clear();
    program_loaded_ = false;
    state_modified_ = false;
    state_modified_label_->copy_label("");
    memory_center_ = 0x3000;
    memory_jump_input_->value("x3000");
    breakpoint_input_->value("x3003");
    simulator_.clearMachine();
    simulator_.clearBreakpoints();
    refreshSimulatorViews();
    appendLog("Loaded example: " + title);
    if (example.description && *example.description) {
        appendLog(std::string("Example note: ") + example.description);
    }
    setStatus("Example loaded: " + title);
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

void MainWindow::beginRegisterEdit(int row) {
    if (!program_loaded_) {
        setStatus("Load program before editing machine state");
        return;
    }
    if (!register_table_->editableRow(row)) {
        setStatus(row == 12 ? "Use Run/Pause to change RUNNING" : "This register row is not editable");
        return;
    }

    stopRunTimer();
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!register_table_->cellBounds(row, 1, x, y, width, height)) {
        setStatus("Register cell is not visible");
        return;
    }

    cell_edit_kind_ = 1;
    cell_edit_row_ = row;
    cell_editor_->resize(x + 1, y + 1, std::max(1, width - 2), std::max(1, height - 2));
    cell_editor_->value(register_table_->valueAtRow(row).c_str());
    cell_editor_->show();
    cell_editor_->take_focus();
    cell_editor_->insert_position(0, cell_editor_->size());
}

void MainWindow::beginMemoryEdit(int row, int col) {
    if (!program_loaded_) {
        setStatus("Load program before editing memory");
        return;
    }

    int current_value = 0;
    if (!memory_table_->valueAtRow(row, current_value)) {
        setStatus("Memory row is not available");
        return;
    }

    stopRunTimer();
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!memory_table_->cellBounds(row, col, x, y, width, height)) {
        setStatus("Memory cell is not visible");
        return;
    }

    cell_edit_kind_ = 2;
    cell_edit_row_ = row;
    cell_editor_->resize(x + 1, y + 1, std::max(1, width - 2), std::max(1, height - 2));
    std::string current = (col == 3) ? ("b" + lc3::formatBinaryWord(current_value))
                                     : lc3::formatHexWord(current_value);
    cell_editor_->value(current.c_str());
    cell_editor_->show();
    cell_editor_->take_focus();
    cell_editor_->insert_position(0, cell_editor_->size());
}

void MainWindow::commitCellEdit() {
    if (!cell_editor_ || !cell_editor_->visible()) {
        return;
    }

    bool ok = false;
    std::string text = cell_editor_->value() ? cell_editor_->value() : "";
    if (cell_edit_kind_ == 1) {
        ok = applyRegisterEdit(cell_edit_row_, text);
    } else if (cell_edit_kind_ == 2) {
        ok = applyMemoryEdit(cell_edit_row_, text);
    }

    if (ok) {
        cancelCellEdit();
    } else {
        cell_editor_->take_focus();
        cell_editor_->insert_position(0, cell_editor_->size());
    }
}

bool MainWindow::applyRegisterEdit(int row, const std::string& text) {
    std::string name = register_table_->nameAtRow(row);
    lc3::OperationResult result;
    if (row >= 0 && row < 8) {
        int value = 0;
        if (!lc3::parseAddress(text, value)) {
            setStatus("Invalid register value");
            appendLog("Invalid register value for " + name + ": " + text);
            fl_alert("Invalid register value for %s.", name.c_str());
            return false;
        }
        result = simulator_.setRegisterValue(row, value);
    } else if (row == 8) {
        int value = 0;
        if (!lc3::parseAddress(text, value)) {
            setStatus("Invalid PC value");
            appendLog("Invalid PC value: " + text);
            fl_alert("Invalid PC value.");
            return false;
        }
        result = simulator_.setPC(value);
        if (result.ok) {
            autoScrollMemoryToPc(simulator_.registers());
        }
    } else if (row == 9) {
        int value = 0;
        if (!lc3::parseAddress(text, value)) {
            setStatus("Invalid IR value");
            appendLog("Invalid IR value: " + text);
            fl_alert("Invalid IR value.");
            return false;
        }
        result = simulator_.setIR(value);
    } else if (row == 10) {
        result = simulator_.setConditionCode(text);
    } else if (row == 13) {
        bool value = false;
        if (!ui::parseBoolText(text, value)) {
            setStatus("Invalid HALTED value");
            appendLog("Invalid HALTED value: " + text);
            fl_alert("Invalid HALTED value. Use true/false or 1/0.");
            return false;
        }
        result = simulator_.setHalted(value);
    } else {
        setStatus("This register row is not editable");
        return false;
    }

    refreshSimulatorViews();
    if (!result.ok) {
        appendLog("State edit failed: " + result.message);
        setStatus("State edit failed");
        fl_alert("%s", result.message.c_str());
        return false;
    }

    markStateModified(name + "=" + register_table_->valueAtRow(row));
    return true;
}

bool MainWindow::applyMemoryEdit(int row, const std::string& text) {
    int address = 0;
    if (!memory_table_->addressAtRow(row, address)) {
        setStatus("Memory row is not available");
        return false;
    }
    int current_value = 0;
    if (!memory_table_->valueAtRow(row, current_value)) {
        setStatus("Memory row is not available");
        return false;
    }
    (void)current_value;

    int value = 0;
    std::string address_text = lc3::formatHexWord(address);
    if (!lc3::parseAddress(text, value)) {
        setStatus("Invalid memory value");
        appendLog("Invalid memory value at " + address_text + ": " + text);
        fl_alert("Invalid memory value at %s.", address_text.c_str());
        return false;
    }

    lc3::OperationResult result = simulator_.setMemoryValue(address, value);
    refreshSimulatorViews();
    if (!result.ok) {
        appendLog("Memory edit failed: " + result.message);
        setStatus("Memory edit failed");
        fl_alert("%s", result.message.c_str());
        return false;
    }

    markStateModified("MEM[" + address_text + "]=" + lc3::formatHexWord(value));
    return true;
}

void MainWindow::cancelCellEdit() {
    if (!cell_editor_) {
        return;
    }
    cell_editor_->hide();
    cell_edit_kind_ = 0;
    cell_edit_row_ = -1;
}

void MainWindow::markStateModified(const std::string& detail) {
    state_modified_ = true;
    std::string notice = "  state modified";
    if (!detail.empty()) {
        notice += ": " + detail;
    }
    state_modified_label_->copy_label(notice.c_str());
    appendLog("Machine state manually modified: " + detail);
    setStatus("Machine state manually modified");
}
