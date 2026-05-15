#include "main_window_internal.h"

using namespace gui::main_window_detail;

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

void MainWindow::onJumpMemoryToPc(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->jumpMemoryToPc();
}

void MainWindow::onAutoMemoryScrollChanged(Fl_Widget* widget, void* data) {
    auto* check = static_cast<Fl_Check_Button*>(widget);
    static_cast<MainWindow*>(data)->setAutoMemoryScroll(check->value() != 0);
}

void MainWindow::onRunRateSliderChanged(Fl_Widget* widget, void* data) {
    auto* slider = static_cast<Fl_Slider*>(widget);
    static_cast<MainWindow*>(data)->setRunRateLimit(sliderValueToRunRate(slider->value()), false);
}

void MainWindow::onRunRateInputChanged(Fl_Widget* widget, void* data) {
    int value = 0;
    auto* input = static_cast<Fl_Input*>(widget);
    auto* self = static_cast<MainWindow*>(data);
    if (!parseRunRateText(input->value(), value)) {
        self->updateRunRateControls();
        self->setStatus("Invalid run rate limit");
        self->appendLog("Invalid run rate limit; use 1-" +
                        std::to_string(kMaxRunRateLimit) + " instructions/s");
        fl_alert("Run rate limit must be an integer from 1 to %d.", kMaxRunRateLimit);
        return;
    }
    self->setRunRateLimit(value, true);
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

void MainWindow::onOpenExample(Fl_Widget* widget, void* data) {
    auto* example = static_cast<const embedded_examples::AssemblyExample*>(data);
    if (!widget || !example) {
        return;
    }

    auto* self = static_cast<MainWindow*>(widget->window());
    self->openExampleProgram(*example);
}

void MainWindow::onCellEditConfirmed(Fl_Widget*, void* data) {
    static_cast<MainWindow*>(data)->commitCellEdit();
}

void MainWindow::onSourceLineDoubleClicked(int source_line, void* data) {
    static_cast<MainWindow*>(data)->toggleBreakpointAtSourceLine(source_line);
}

void MainWindow::onRegisterTableEvent(Fl_Widget* widget, void* data) {
    auto* self = static_cast<MainWindow*>(data);
    auto* table = static_cast<RegisterTable*>(widget);
    if (table->callback_context() != Fl_Table::CONTEXT_CELL || Fl::event_clicks() == 0) {
        return;
    }

    if (table->callback_col() == 1) {
        self->beginRegisterEdit(table->callback_row());
    }
    Fl::event_clicks(0);
}

void MainWindow::onMemoryTableEvent(Fl_Widget* widget, void* data) {
    auto* self = static_cast<MainWindow*>(data);
    auto* table = static_cast<MemoryTable*>(widget);
    if (table->callback_context() != Fl_Table::CONTEXT_CELL || Fl::event_clicks() == 0) {
        return;
    }

    int address = 0;
    if (table->addressAtRow(table->callback_row(), address)) {
        if (table->callback_col() == 0) {
            self->toggleBreakpointAtAddress(address);
        } else if (table->callback_col() == 2 || table->callback_col() == 3) {
            self->beginMemoryEdit(table->callback_row(), table->callback_col());
        } else {
            self->setStatus("Only Hex and Binary memory columns are editable");
        }
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
