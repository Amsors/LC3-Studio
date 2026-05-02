#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/fl_ask.H>

static void on_login_click(Fl_Widget*, void*) {
    fl_message("FLTK 环境已就绪。\n下一步：将网络通登录逻辑迁移到 C++。");
    fl_message("123");
}

int main(int argc, char** argv) {
    Fl::scheme("gtk+");

    Fl_Window* win = new Fl_Window(420, 220, "wlt_helper_cpp - FLTK Hello");

    Fl_Box* title = new Fl_Box(20, 20, 380, 40, "网络通助手 (C++/FLTK)");
    title->labelsize(20);
    title->align(FL_ALIGN_CENTER);

    Fl_Box* info = new Fl_Box(20, 70, 380, 60,
        "FLTK 1.4.5 静态库 + 静态 CRT (/MT)\n"
        "exe 单文件运行，无需任何 DLL");
    info->labelsize(12);
    info->align(FL_ALIGN_CENTER | FL_ALIGN_WRAP);

    Fl_Button* btn = new Fl_Button(140, 150, 140, 40, "测试一下");
    btn->callback(on_login_click);

    win->end();
    win->show(argc, argv);
    return Fl::run();
}
