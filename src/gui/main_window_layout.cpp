#include "main_window_internal.h"

using namespace gui::main_window_detail;

void MainWindow::buildUi() {
    menu_ = new Fl_Menu_Bar(0, 0, w(), kMenuHeight);
    menu_->add("&File/&Open...\tCtrl+O", FL_CTRL + 'o', onOpen, this);
    if (embedded_examples::exampleCount() == 0) {
        menu_->add("&File/Open &Example/(no embedded examples)", 0, nullptr, nullptr, FL_MENU_INACTIVE);
    } else {
        for (std::size_t i = 0; i < embedded_examples::exampleCount(); i++) {
            const embedded_examples::AssemblyExample* example = embedded_examples::exampleAt(i);
            if (!example) {
                continue;
            }
            std::string path = "&File/Open &Example/" + exampleMenuTitle(*example);
            menu_->add(path.c_str(), 0, onOpenExample,
                       const_cast<embedded_examples::AssemblyExample*>(example));
        }
    }
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
    state_modified_label_ = new Fl_Box(0, 0, 1, kButtonHeight, "");
    state_modified_label_->box(FL_FLAT_BOX);
    state_modified_label_->color(fl_rgb_color(255, 248, 220));
    state_modified_label_->labelcolor(fl_rgb_color(139, 75, 0));
    state_modified_label_->labelfont(FL_HELVETICA_BOLD);
    state_modified_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    memory_jump_input_ = new EnterCallbackInput(kMargin + 780, kMenuHeight + 9, 104, kButtonHeight, "");
    memory_jump_input_->value("x3000");
    memory_jump_input_->callback(onJumpMemory, this);
    jump_button_ = new Fl_Button(kMargin + 892, kMenuHeight + 9, 80, kButtonHeight, "Jump");
    jump_button_->callback(onJumpMemory, this);
    jump_pc_button_ = new Fl_Button(0, 0, 1, kButtonHeight, "To PC");
    jump_pc_button_->callback(onJumpMemoryToPc, this);
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
    auto_memory_scroll_check_ = new Fl_Check_Button(0, 0, 1, 1, "Auto PC scroll");
    auto_memory_scroll_check_->value(1);
    auto_memory_scroll_check_->callback(onAutoMemoryScrollChanged, this);

    jump_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    breakpoint_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    source_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    machine_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    register_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    memory_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    auto_memory_scroll_check_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    jump_label_->labelfont(FL_BOLD);
    breakpoint_label_->labelfont(FL_BOLD);
    source_label_->labelfont(FL_BOLD);
    machine_label_->labelfont(FL_BOLD);
    register_label_->labelfont(FL_BOLD);
    memory_label_->labelfont(FL_BOLD);

    editor_buffer_ = new Fl_Text_Buffer();
    editor_style_buffer_ = new Fl_Text_Buffer();
    machine_buffer_ = new Fl_Text_Buffer();
    trap_input_buffer_ = new Fl_Text_Buffer();
    trap_output_buffer_ = new Fl_Text_Buffer();
    log_buffer_ = new Fl_Text_Buffer();

    editor_ = new AsmSourceEditor(0, 0, 1, 1);
    editor_->buffer(editor_buffer_);
    editor_->textfont(FL_COURIER);
    editor_->textsize(14);
    editor_->linenumber_width(62);
    editor_->linenumber_align(FL_ALIGN_RIGHT);
    editor_->linenumber_size(12);
    static_cast<AsmSourceEditor*>(editor_)->sourceLineDoubleClickCallback(onSourceLineDoubleClicked,
                                                                           this);
    editor_->highlight_data(editor_style_buffer_,
                            ui::kAsmStyleTable,
                            ui::kAsmStyleTableSize,
                            ui::kAsmStyleDefault,
                            nullptr,
                            nullptr);
    editor_buffer_->add_modify_callback(onTextModified, this);

    machine_display_ = new WheelIsolatedTextDisplay(0, 0, 1, 1);
    machine_display_->buffer(machine_buffer_);
    machine_display_->textfont(FL_COURIER);
    machine_display_->textsize(14);

    register_table_ = new RegisterTable(0, 0, 1, 1);
    register_table_->callback(onRegisterTableEvent, this);
    register_table_->when(FL_WHEN_RELEASE_ALWAYS);
    register_table_->type(Fl_Table_Row::SELECT_SINGLE);
    memory_table_ = new MemoryTable(0, 0, 1, 1);
    memory_table_->callback(onMemoryTableEvent, this);
    memory_table_->when(FL_WHEN_RELEASE_ALWAYS);
    memory_table_->type(Fl_Table_Row::SELECT_SINGLE);

    cell_editor_ = new Fl_Input(0, 0, 1, 1, "");
    cell_editor_->box(FL_THIN_DOWN_BOX);
    cell_editor_->textfont(FL_COURIER);
    cell_editor_->textsize(13);
    cell_editor_->when(FL_WHEN_ENTER_KEY_ALWAYS);
    cell_editor_->callback(onCellEditConfirmed, this);
    cell_editor_->hide();

    bottom_tabs_ = new Fl_Tabs(0, 0, 1, 1);
    bottom_tabs_->begin();

    io_tab_ = new Fl_Group(0, 0, 1, 1, "I/O");
    io_tab_->begin();

    trap_input_label_ = new Fl_Box(0, 0, 1, 1, "TRAP Input Buffer");
    trap_output_label_ = new Fl_Box(0, 0, 1, 1, "TRAP Output");
    trap_remaining_label_ = new Fl_Box(0, 0, 1, 1, "Remaining: 0");
    log_label_ = new Fl_Box(0, 0, 1, 1, "Log");
    trap_input_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    trap_output_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    trap_remaining_label_->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
    log_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    trap_input_label_->labelfont(FL_BOLD);
    trap_output_label_->labelfont(FL_BOLD);
    log_label_->labelfont(FL_BOLD);

    trap_input_editor_ = new WheelIsolatedTextEditor(0, 0, 1, 1);
    trap_input_editor_->buffer(trap_input_buffer_);
    trap_input_editor_->textfont(FL_COURIER);
    trap_input_editor_->textsize(13);
    trap_input_buffer_->add_modify_callback(onTrapInputModified, this);

    trap_output_display_ = new WheelIsolatedTextDisplay(0, 0, 1, 1);
    trap_output_display_->buffer(trap_output_buffer_);
    trap_output_display_->textfont(FL_COURIER);
    trap_output_display_->textsize(13);

    clear_trap_output_button_ = new Fl_Button(0, 0, 1, 1, "Clear");
    clear_trap_output_button_->callback(onClearTrapOutput, this);

    log_display_ = new WheelIsolatedTextDisplay(0, 0, 1, 1);
    log_display_->buffer(log_buffer_);
    log_display_->textfont(FL_COURIER);
    log_display_->textsize(13);

    io_tab_->end();

    settings_tab_ = new Fl_Group(0, 0, 1, 1, "Settings");
    settings_tab_->begin();

    run_rate_label_ = new Fl_Box(0, 0, 1, 1, "Run rate limit");
    run_rate_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    run_rate_label_->labelfont(FL_BOLD);

    run_rate_input_ = new Fl_Input(0, 0, 1, 1, "");
    run_rate_input_->type(FL_INT_INPUT);
    run_rate_input_->when(FL_WHEN_ENTER_KEY_ALWAYS);
    run_rate_input_->callback(onRunRateInputChanged, this);

    run_rate_slider_ = new Fl_Slider(0, 0, 1, 1);
    run_rate_slider_->type(FL_HORIZONTAL);
    run_rate_slider_->bounds(kRunRateSliderMin, kRunRateSliderMax);
    run_rate_slider_->step(0.001);
    run_rate_slider_->when(FL_WHEN_CHANGED);
    run_rate_slider_->callback(onRunRateSliderChanged, this);

    run_rate_unit_label_ = new Fl_Box(0, 0, 1, 1, "instructions/s");
    run_rate_unit_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    settings_tab_->end();
    bottom_tabs_->end();
    updateRunRateControls();

    status_bar_ = new Fl_Box(0, 0, w(), kStatusHeight, "");
    status_bar_->box(FL_FLAT_BOX);
    status_bar_->color(fl_rgb_color(238, 238, 238));
    status_bar_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);

    end();
    layoutChildren(w(), h());
}

void MainWindow::layoutChildren(int width, int height) {
    cancelCellEdit();
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
    button_x += 72 + kGap;
    int notice_w = std::max(1, width - button_x - kMargin);
    state_modified_label_->resize(button_x, row1_y, notice_w, kButtonHeight);

    button_x = kMargin;
    jump_label_->resize(button_x, row2_y, 62, kButtonHeight);
    button_x += 62 + kGap;
    int jump_input_w = std::min(116, std::max(86, width / 10));
    memory_jump_input_->resize(button_x, row2_y, jump_input_w, kButtonHeight);
    button_x += jump_input_w + kGap;
    jump_button_->resize(button_x, row2_y, 76, kButtonHeight);
    button_x += 76 + kGap;
    jump_pc_button_->resize(button_x, row2_y, 64, kButtonHeight);
    button_x += 64 + 2 * kGap;

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
    int bottom_h = std::max(132, content_h * 24 / 100);
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

    int auto_scroll_w = std::min(148, std::max(112, content_w / 6));
    memory_label_->resize(content_x, memory_y, std::max(1, content_w - auto_scroll_w - kGap),
                          kLabelHeight);
    auto_memory_scroll_check_->resize(content_x + std::max(0, content_w - auto_scroll_w),
                                      memory_y + 1, auto_scroll_w, std::max(1, kLabelHeight - 2));
    memory_table_->resize(content_x, memory_y + kLabelHeight, content_w,
                          std::max(1, memory_h - kLabelHeight));
    memory_table_->fitColumns(content_w);

    bottom_tabs_->resize(content_x, bottom_y, content_w, bottom_h);
    int bottom_page_y = bottom_y + kTabHeaderHeight;
    int bottom_page_h = std::max(1, bottom_h - kTabHeaderHeight);
    io_tab_->resize(content_x, bottom_page_y, content_w, bottom_page_h);
    settings_tab_->resize(content_x, bottom_page_y, content_w, bottom_page_h);

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
    int bottom_body_y = bottom_page_y + kLabelHeight;
    int bottom_body_h = std::max(1, bottom_page_h - kLabelHeight);

    trap_input_label_->resize(trap_input_x, bottom_page_y, std::max(1, trap_input_w - 108), kLabelHeight);
    trap_remaining_label_->resize(trap_input_x + std::max(1, trap_input_w - 108), bottom_page_y,
                                  std::min(108, trap_input_w), kLabelHeight);
    trap_input_editor_->resize(trap_input_x, bottom_body_y, trap_input_w, bottom_body_h);

    trap_output_label_->resize(trap_output_x, bottom_page_y, std::max(1, trap_output_w - 72), kLabelHeight);
    clear_trap_output_button_->resize(trap_output_x + std::max(0, trap_output_w - 64),
                                      bottom_page_y + 2, std::min(64, trap_output_w), kLabelHeight - 4);
    trap_output_display_->resize(trap_output_x, bottom_body_y, trap_output_w, bottom_body_h);

    log_label_->resize(log_x, bottom_page_y, log_w, kLabelHeight);
    log_display_->resize(log_x, bottom_body_y, log_w, bottom_body_h);

    int settings_x = content_x + kGap;
    int settings_y = bottom_page_y + 12;
    int settings_w = std::max(1, content_w - 2 * kGap);
    int rate_label_w = std::min(150, std::max(120, settings_w / 5));
    int rate_input_w = 104;
    int rate_unit_w = 120;
    run_rate_label_->resize(settings_x, settings_y, rate_label_w, kButtonHeight);
    run_rate_input_->resize(settings_x + rate_label_w + kGap, settings_y,
                            rate_input_w, kButtonHeight);
    run_rate_unit_label_->resize(settings_x + rate_label_w + rate_input_w + 2 * kGap,
                                 settings_y, rate_unit_w, kButtonHeight);
    int slider_y = settings_y + kButtonHeight + 14;
    run_rate_slider_->resize(settings_x, slider_y, settings_w,
                             std::max(1, bottom_page_y + bottom_page_h - slider_y - 8));

    status_bar_->resize(0, height - kStatusHeight, width, kStatusHeight);
}
