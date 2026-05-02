#pragma once

#include "LC3/lc3_gui_adapter.h"

#include <FL/Fl_Double_Window.H>

#include <filesystem>
#include <string>
#include <vector>

class Fl_Box;
class Fl_Button;
class Fl_Menu_Bar;
class Fl_Text_Buffer;
class Fl_Text_Display;
class Fl_Text_Editor;
class Fl_Widget;

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
    static void onCoreSelfTest(Fl_Widget* widget, void* data);
    static void onTextModified(int pos, int inserted, int deleted, int restyled,
                               const char* deleted_text, void* data);
    static void onClose(Fl_Widget* widget, void* data);

    void buildUi();
    void layoutChildren(int width, int height);

    void openFile();
    bool saveFile();
    bool saveFileAs();
    void assembleSource();
    void runCoreSelfTest();
    void requestClose();

    void setEditorText(const std::string& text);
    std::string editorText() const;
    void setMachineOutput(const std::string& text);
    void appendLog(const std::string& message);
    void setStatus(const std::string& message);
    void updateTitle();

    static std::string defaultSource();
    static std::string fileDisplayName(const std::filesystem::path& path);
    static std::string formatMachineOutput(const std::vector<std::string>& words);
    static int parseBinaryWord(const std::string& word);

    lc3::AssemblerService assembler_;

    Fl_Menu_Bar* menu_ = nullptr;
    Fl_Button* open_button_ = nullptr;
    Fl_Button* save_button_ = nullptr;
    Fl_Button* assemble_button_ = nullptr;
    Fl_Box* source_label_ = nullptr;
    Fl_Box* machine_label_ = nullptr;
    Fl_Box* log_label_ = nullptr;
    Fl_Text_Editor* editor_ = nullptr;
    Fl_Text_Display* machine_display_ = nullptr;
    Fl_Text_Display* log_display_ = nullptr;
    Fl_Box* status_bar_ = nullptr;

    Fl_Text_Buffer* editor_buffer_ = nullptr;
    Fl_Text_Buffer* machine_buffer_ = nullptr;
    Fl_Text_Buffer* log_buffer_ = nullptr;

    std::filesystem::path current_file_;
    bool dirty_ = false;
    bool programmatic_edit_ = false;
};
