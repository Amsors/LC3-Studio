#include "main_window_internal.h"

using namespace gui::main_window_detail;

MainWindow::MainWindow(int width, int height)
    : Fl_Double_Window(width, height, "LC-3 Studio"),
      run_rate_limit_(kDefaultRunRateLimit) {
    buildUi();
    const embedded_examples::AssemblyExample* startup_example = embedded_examples::exampleAt(0);
    if (startup_example) {
        setEditorText(startup_example->source ? startup_example->source : "");
        current_example_title_ = startup_example->title ? startup_example->title : "";
    } else {
        setEditorText(defaultSource());
    }
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
    cancelCellEdit();

    if (editor_buffer_) {
        editor_buffer_->remove_modify_callback(onTextModified, this);
    }
    if (trap_input_buffer_) {
        trap_input_buffer_->remove_modify_callback(onTrapInputModified, this);
    }

    clear();

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

