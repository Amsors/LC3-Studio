#pragma once

#include "LC3/lc3_gui_adapter.h"

#include <FL/Fl_Double_Window.H>

#include <filesystem>
#include <string>
#include <vector>

class Fl_Box;
class Fl_Button;
class Fl_Input;
class Fl_Menu_Bar;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Text_Editor;
class Fl_Widget;
class MemoryTable;
class RegisterTable;

class MainWindow : public Fl_Double_Window {
public:
    MainWindow(int width, int height);
    ~MainWindow() override;

    void resize(int x, int y, int width, int height) override;

private:
    static void onOpen(Fl_Widget* widget, void* data);
    static void onSave(Fl_Widget* widget, void* data);
    static void onSaveAs(Fl_Widget* widget, void* data);
    static void onExit(Fl_Widget* widget, void* data);
    static void onAssemble(Fl_Widget* widget, void* data);
    static void onAssembleAndLoad(Fl_Widget* widget, void* data);
    static void onRun(Fl_Widget* widget, void* data);
    static void onPause(Fl_Widget* widget, void* data);
    static void onStep(Fl_Widget* widget, void* data);
    static void onReset(Fl_Widget* widget, void* data);
    static void onJumpMemory(Fl_Widget* widget, void* data);
    static void onClearTrapOutput(Fl_Widget* widget, void* data);
    static void onCoreSelfTest(Fl_Widget* widget, void* data);
    static void onTextModified(int pos, int inserted, int deleted, int restyled,
                               const char* deleted_text, void* data);
    static void onTrapInputModified(int pos, int inserted, int deleted, int restyled,
                                    const char* deleted_text, void* data);
    static void onClose(Fl_Widget* widget, void* data);
    static void onRunTimer(void* data);

    void buildUi();
    void layoutChildren(int width, int height);

    void openFile();
    bool saveFile();
    bool saveFileAs();
    void assembleSource();
    void assembleAndLoad();
    void runProgram();
    void pauseProgram();
    void stopRunTimer();
    void runTimerTick();
    void stepOnce();
    void resetProgram();
    void jumpMemory();
    void clearTrapOutput();
    void runCoreSelfTest();
    void requestClose();

    void setEditorText(const std::string& text);
    std::string editorText() const;
    std::string trapInputText() const;
    void setMachineOutput(const std::string& text);
    void appendLog(const std::string& message);
    void setStatus(const std::string& message);
    void updateTrapInputBuffer();
    void refreshSimulatorViews();
    void refreshRegisterView(const lc3::RegisterView& registers);
    void refreshMemoryView();
    void refreshTrapViews();
    void updateControlStates(const lc3::RegisterView& registers);
    void updateTitle();

    static std::string defaultSource();
    static std::string fileDisplayName(const std::filesystem::path& path);
    static std::string formatMachineOutput(const std::vector<std::string>& words);
    static int parseBinaryWord(const std::string& word);

    lc3::AssemblerService assembler_;
    lc3::SimulatorService simulator_;

    Fl_Menu_Bar* menu_ = nullptr;
    Fl_Button* open_button_ = nullptr;
    Fl_Button* save_button_ = nullptr;
    Fl_Button* assemble_button_ = nullptr;
    Fl_Button* load_button_ = nullptr;
    Fl_Button* run_button_ = nullptr;
    Fl_Button* pause_button_ = nullptr;
    Fl_Button* step_button_ = nullptr;
    Fl_Button* reset_button_ = nullptr;
    Fl_Button* jump_button_ = nullptr;
    Fl_Button* clear_trap_output_button_ = nullptr;
    Fl_Input* memory_jump_input_ = nullptr;
    Fl_Box* source_label_ = nullptr;
    Fl_Box* machine_label_ = nullptr;
    Fl_Box* register_label_ = nullptr;
    Fl_Box* memory_label_ = nullptr;
    Fl_Box* trap_input_label_ = nullptr;
    Fl_Box* trap_output_label_ = nullptr;
    Fl_Box* trap_remaining_label_ = nullptr;
    Fl_Box* log_label_ = nullptr;
    Fl_Text_Editor* editor_ = nullptr;
    Fl_Text_Display* machine_display_ = nullptr;
    RegisterTable* register_table_ = nullptr;
    MemoryTable* memory_table_ = nullptr;
    Fl_Text_Editor* trap_input_editor_ = nullptr;
    Fl_Text_Display* trap_output_display_ = nullptr;
    Fl_Text_Display* log_display_ = nullptr;
    Fl_Box* status_bar_ = nullptr;

    Fl_Text_Buffer* editor_buffer_ = nullptr;
    Fl_Text_Buffer* machine_buffer_ = nullptr;
    Fl_Text_Buffer* trap_input_buffer_ = nullptr;
    Fl_Text_Buffer* trap_output_buffer_ = nullptr;
    Fl_Text_Buffer* log_buffer_ = nullptr;

    std::filesystem::path current_file_;
    std::string latest_machine_code_;
    int memory_center_ = 0x3000;
    bool program_loaded_ = false;
    bool dirty_ = false;
    bool programmatic_edit_ = false;
    bool run_timer_active_ = false;
};
