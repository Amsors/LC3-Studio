# LC-3 图形化汇编器与模拟器开发规格说明

## 1. 项目目标

使用 FLTK 为现有的 LC-3 汇编器和模拟器开发一个 Windows 桌面图形界面。

本应用面向《面向对象程序设计》课程大作业和课堂展示。最终的 Release 可执行文件应当能够在教室 Windows 电脑上直接运行，不需要额外安装运行时环境。

主要用户流程：

1. 在图形界面中编辑 LC-3 `.asm` 源代码。
2. 将源代码汇编为 LC-3 机器码。
3. 将机器码加载到模拟器中。
4. 通过单步执行、运行/暂停、内存地址断点、寄存器视图、内存视图以及 TRAP 输入/输出缓冲区进行调试。

## 2. 已确认需求

- 图形界面只需要编辑 `.asm` 源代码。
- 第一版不要求直接编辑或加载原始 16 位机器码。
- 单步调试只需要高亮对应的内存地址/机器字。
- TRAP 输入来自图形界面中预设的输入缓冲区。
- 第一版不要求在运行时修改内存或寄存器。
- 断点按内存地址设置，而不是按源代码行设置。
- GUI 必须使用 FLTK。
- 开发和调试目标环境为 Visual Studio 2022。
- Release 构建应尽可能静态链接运行时，尤其是使用 `/MT`。

## 3. 第一版不做的内容

- 不支持源代码行断点。
- 不支持实时编辑寄存器或内存。
- 不实现完整 IDE 功能，例如语法高亮、自动补全或项目管理。
- 不支持当前 `x3000` 行为之外的自定义起始地址。
- 不支持多文件汇编项目。
- 不实现周期精确的硬件 I/O 模拟。

## 4. 建议架构

应用建议分为三层：

```text
FLTK GUI Layer
  |
  v
Application Controller Layer
  |
  +-- AssemblerService
  +-- SimulatorService
  +-- FileService
  |
  v
LC-3 Core Layer
  +-- Parser
  +-- StateMachine
```

### 4.1 LC-3 核心层

该层包含 `LC3` 目录中现有的汇编器和模拟器逻辑。

职责：

- 解析并汇编 `.asm` 源代码。
- 将机器码加载到内存。
- 精确执行一条指令。
- 维护 LC-3 状态：内存、寄存器、PC、IR、CC、停止/运行状态。
- 为 GUI 暴露只读状态访问接口。
- 暴露 TRAP 输入/输出回调。

当前可用接口：

- `Parser::AssembleSource(...)`
- `Parser::AssembleSourceToWords(...)`
- `StateMachine::LoadCodeFromString(...)`
- `StateMachine::StepOnce()`
- `StateMachine::GetSnapshot()`
- `StateMachine::ReadMemoryRange(...)`
- `StateMachine::SetReadCharHandler(...)`
- `StateMachine::SetWriteCharHandler(...)`

建议重构结构：

```text
LC3/
  assembler.h
  assembler.cpp
  runtime.h
  runtime.cpp
  lc3_types.h
  config.json
```

`assembler.cpp` 和 `runtime.cpp` 中现有的 `main()` 函数应只用于控制台测试，并在 GUI 构建中通过类似下面的库模式宏排除：

```cpp
#ifndef LC3_AS_LIBRARY
int main() {
    ...
}
#endif
```

### 4.2 应用控制层

该层负责把 GUI 事件连接到 LC-3 操作。

建议文件：

```text
src/
  app_controller.h
  app_controller.cpp
  assembler_service.h
  assembler_service.cpp
  simulator_service.h
  simulator_service.cpp
  file_service.h
  file_service.cpp
```

#### AssemblerService

职责：

- 从编辑器接收源代码文本。
- 调用 `Parser`。
- 返回汇编后的机器字和错误信息。
- 可选：构建地址到机器字的显示模型。

建议 API：

```cpp
struct AssembleResult {
    bool ok = false;
    std::string machine_code;
    std::vector<std::string> words;
    std::string error_message;
};

class AssemblerService {
public:
    AssembleResult assembleSource(const std::string& source);
};
```

#### SimulatorService

职责：

- 持有一个 `StateMachine`。
- 加载汇编后的机器码。
- 执行单步。
- 在 GUI 定时器控制下执行多步。
- 管理地址断点。
- 提供寄存器和内存视图模型。
- 管理预设 TRAP 输入缓冲区和输出缓冲区。

建议 API：

```cpp
struct RegisterView {
    int r[8] = {};
    int pc = 0;
    int ir = 0;
    std::string cc;
    bool halted = false;
    bool running = false;
};

struct MemoryRow {
    int address = 0;
    int value = 0;
    bool is_pc = false;
    bool has_breakpoint = false;
};

class SimulatorService {
public:
    void loadMachineCode(const std::string& machine_code);
    void reset();

    bool stepOnce();
    bool stepForRun();

    void setRunning(bool value);
    bool isRunning() const;
    bool isHalted() const;

    void setBreakpoint(int address, bool enabled);
    bool hasBreakpoint(int address) const;

    RegisterView registers() const;
    std::vector<MemoryRow> memoryWindow(int center_address, int before, int after) const;

    void setTrapInputBuffer(const std::string& text);
    std::string trapOutputBuffer() const;
    void clearTrapOutputBuffer();
};
```

#### AppController

职责：

- 持有 `AssemblerService` 和 `SimulatorService`。
- 协调 GUI 状态切换。
- 保存最新源代码文本、机器码、当前内存焦点地址和状态消息。
- 将异常转换为面向用户的错误消息。

示例命令：

```cpp
void assemble();
void assembleAndLoad();
void step();
void run();
void pause();
void reset();
void toggleBreakpointAtAddress(int address);
void updateTrapInput(const std::string& input);
```

### 4.3 FLTK GUI 层

建议文件：

```text
src/ui/
  main_window.h
  main_window.cpp
  editor_view.h
  editor_view.cpp
  memory_view.h
  memory_view.cpp
  register_view.h
  register_view.cpp
  console_view.h
  console_view.cpp
```

建议控件：

```text
Fl_Double_Window       主窗口
Fl_Menu_Bar            文件/构建/运行菜单
Fl_Button              汇编、加载、运行、暂停、单步、重置按钮
Fl_Text_Editor         ASM 源代码编辑器
Fl_Text_Display        机器码、日志、TRAP 输出
Fl_Table_Row           内存视图和寄存器视图
Fl_Input               内存地址输入和 TRAP 输入缓冲区
Fl_Native_File_Chooser 打开/保存 .asm 文件
Fl_Tabs                日志 / 机器码 / TRAP 输出
```

## 5. GUI 布局

推荐的第一版布局：

```text
+--------------------------------------------------------------+
| Menu: File  Build  Run  View                                 |
+--------------------------------------------------------------+
| [Assemble] [Load] [Run] [Pause] [Step] [Reset] [Jump x____]  |
+--------------------------+-------------------+---------------+
| ASM Editor               | Machine Code      | Registers     |
|                          | / Status          | R0..R7        |
|                          |                   | PC IR CC      |
+--------------------------+-------------------+---------------+
| Memory View                                                  |
| Address | Hex | Binary | Flags                               |
+--------------------------------------------------------------+
| TRAP Input Buffer | TRAP Output | Log                         |
+--------------------------------------------------------------+
```

寄存器视图应始终显示：

```text
R0-R7
PC
IR
CC
RUNNING
HALTED
```

内存视图应显示有限窗口，而不是显示全部 65536 行。

推荐默认行为：

- 显示 `PC - 8` 到 `PC + 40`。
- 将地址限制在 `[x0000, xFFFF]` 范围内。
- 高亮当前 `PC`。
- 使用可见标记显示断点，例如 `BP`。
- 地址显示为 `x3000` 格式。
- 机器字同时显示为十六进制和二进制。

## 6. 调试执行模型

### 6.1 单步执行

单步行为：

1. 调用 `SimulatorService::stepOnce()`。
2. 刷新寄存器视图。
3. 围绕当前 PC 刷新内存视图。
4. 高亮地址等于 PC 的内存行。
5. 刷新 TRAP 输出。
6. 如果模拟器停止，更新状态并禁用不适用的运行控件。

### 6.2 运行/暂停

不要在 FLTK 回调中执行很长的 `while` 循环，否则会阻塞 GUI 事件循环。

使用 `Fl::add_timeout()` 和 `Fl::repeat_timeout()` 进行协作式执行。

推荐行为：

```text
Run button:
  set running = true
  start FLTK timer

Timer callback:
  execute N instructions, for example 100 per tick
  stop if halted
  stop if PC reaches a breakpoint
  refresh GUI
  reschedule timer if still running

Pause button:
  set running = false
  do not reschedule timer
```

断点处理：

```text
Before executing an instruction during Run:
  if current PC has a breakpoint:
      pause
      refresh GUI
      show status "Paused at breakpoint x...."
      do not execute that instruction
```

对于单步执行，当前 PC 上的断点不应阻止执行。这样用户可以手动单步越过断点。

## 7. TRAP 输入/输出设计

GUI 应提供：

- 一个预设 TRAP 输入文本框。
- 一个 TRAP 输出显示区。
- 一个清空输出按钮。
- 可选：显示剩余输入字符数的指示器。

实现思路：

```cpp
std::string trap_input;
size_t trap_input_pos = 0;
std::string trap_output;

state_machine.SetReadCharHandler([this]() -> int {
    if (trap_input_pos >= trap_input.size()) {
        return 0;
    }
    return static_cast<unsigned char>(trap_input[trap_input_pos++]);
});

state_machine.SetWriteCharHandler([this](unsigned char c) {
    trap_output.push_back(static_cast<char>(c));
});
```

后续待定问题：

- 如果 `GETC` 或 `IN` 执行时输入缓冲区为空，模拟器应该返回 `0`、暂停并请求用户输入，还是报告错误？

第一版采用最简单且确定的行为：

```text
Empty TRAP input returns character value 0.
```

## 8. 断点设计

断点基于内存地址。

建议表示方式：

```cpp
std::set<int> breakpoints;
```

支持的操作：

- 通过地址输入添加断点，例如 `x3005`。
- 通过地址输入移除断点。
- 双击内存行切换断点。
- 清除所有断点。

地址解析器应接受：

```text
x3000
3000
0x3000
#12288
```

如果时间有限，第一版可以只支持 `xNNNN` 格式。

## 9. 错误处理

汇编错误：

- 捕获 `AssemblyError`。
- 在日志面板显示错误消息。
- 汇编失败后不要加载过期机器码。

运行时错误：

- 捕获 `RuntimeError`。
- 暂停执行。
- 在日志面板显示错误消息。
- 保持当前机器状态可见，方便调试。

文件错误：

- 显示对话框和日志消息。
- 不要让应用崩溃。

## 10. 构建和发布要求

Visual Studio 2022 设置：

- 使用 C++17 或更新标准。
- 使用 `/utf-8` 避免源代码文本编码问题。
- Release 运行时链接使用 `/MT`。
- 链接来自 `third_party/fltk/lib/Release` 的 FLTK 静态库。
- 包含来自 `third_party/fltk/include` 的头文件。

推荐的 Release 库：

```text
fltk.lib
fltk_images.lib
fltk_png.lib
fltk_jpeg.lib
fltk_z.lib
comctl32.lib
ws2_32.lib
gdi32.lib
ole32.lib
uuid.lib
shell32.lib
advapi32.lib
user32.lib
```

发布前需要处理 `config.json` 问题。可选方案：

1. 将 `LC3/config.json` 复制到可执行文件旁边。
2. 将整个 `LC3` 文件夹复制到可执行文件旁边。
3. 将指令配置嵌入代码。

对于课堂展示，方案 3 最稳妥。为了快速开发，方案 1 可以接受。

## 11. 分阶段开发计划

### 阶段 0：基线清理

目标：

- 确保现有 FLTK 项目能在 Visual Studio 2022 中干净构建。
- 修复 `src/main.cpp` 中当前的中文文本编码问题。
- 确认 Release `/MT` 静态构建可用。

任务：

- 添加 `/utf-8` 编译选项。
- 验证 FLTK include/library 路径。
- 验证 Debug 和 Release 配置。
- 将乱码示例 UI 文本替换为正常 UTF-8 或 ASCII 文本。

交付物：

- 一个最小 FLTK 窗口可以构建并运行。

### 阶段 1：核心集成

目标：

- 允许 GUI 代码调用现有汇编器和模拟器。

任务：

- 分离或暴露 `Parser`、`AssemblyError`、`StateMachine` 和 `RuntimeError` 的头文件。
- 确保控制台 `main()` 函数在 GUI 构建中被排除。
- 添加 `AssemblerService`。
- 添加 `SimulatorService`。
- 添加一个简单的编译期或运行期测试路径：
  - 汇编一个短 `.asm` 程序；
  - 加载它；
  - 调用 `StepOnce()`；
  - 读取 PC/寄存器快照。

交付物：

- GUI 项目可以编译并链接 LC-3 核心代码。

### 阶段 2：ASM 编辑器和汇编输出

目标：

- 构建第一个可用 GUI 工作流：编辑 `.asm`、汇编、查看机器码。

任务：

- 使用 `Fl_Text_Editor` 添加源代码编辑器。
- 添加汇编按钮。
- 在 `Fl_Text_Display` 中显示机器码输出。
- 在日志面板显示汇编错误。
- 添加打开/保存 `.asm` 文件操作。

交付物：

- 用户可以编辑或打开 `.asm`，进行汇编，并看到生成的 16 位机器字。

### 阶段 3：加载并检查机器状态

目标：

- 将汇编后的代码加载到模拟器并检查状态。

任务：

- 添加 Load 或 Assemble+Load 按钮。
- 显示寄存器 `R0-R7`、`PC`、`IR`、`CC`、`RUNNING`、`HALTED`。
- 显示 `x3000` 附近的内存窗口。
- 添加地址格式化辅助函数。
- 添加内存跳转输入。

交付物：

- 用户可以汇编/加载程序，并检查初始寄存器和内存。

### 阶段 4：单步调试

目标：

- 支持一次执行一条指令的调试。

任务：

- 添加 Step 按钮。
- 调用 `SimulatorService::stepOnce()`。
- 每次单步后刷新寄存器视图。
- 围绕当前 PC 刷新内存视图。
- 高亮当前 PC 所在内存行。
- 执行 `HALT` 后显示停止状态。

交付物：

- 用户可以反复点击 Step，并看到 PC/寄存器/内存状态更新。

### 阶段 5：TRAP 输入和输出缓冲区

目标：

- 支持由 GUI 控制的确定性 TRAP I/O。

任务：

- 添加预设输入缓冲区文本框或多行编辑器。
- 添加输出显示区。
- 接入 `SetReadCharHandler`。
- 接入 `SetWriteCharHandler`。
- 添加清空输出按钮。
- 如果时间允许，显示剩余输入位置。

交付物：

- 使用 `GETC`、`OUT`、`PUTS`、`IN` 和 `PUTSP` 的 LC-3 程序可以与 GUI 缓冲区交互。

### 阶段 6：使用 FLTK 定时器实现运行/暂停

目标：

- 支持连续执行，同时不冻结 GUI。

任务：

- 添加 Run 和 Pause 按钮。
- 使用 `Fl::add_timeout()` 和 `Fl::repeat_timeout()` 实现定时器回调。
- 每个 tick 执行有限数量的指令。
- 在停止或运行时错误时自动停止。
- 每个 tick 后刷新 GUI。

交付物：

- 用户可以运行和暂停程序，同时 GUI 保持响应。

### 阶段 7：地址断点

目标：

- 支持内存地址断点。

任务：

- 添加断点设置/移除控件。
- 在内存表中添加断点标记。
- Run 模式下，在执行断点地址处的指令前停止。
- 允许 Step 在 PC 位于断点时仍然执行。
- 添加清除所有断点操作。

交付物：

- 用户可以运行到指定内存地址，然后检查状态。

### 阶段 8：完善和展示准备

目标：

- 让项目稳定并适合课堂展示。

任务：

- 改进布局尺寸和窗口缩放行为。
- 添加示例 `.asm` 程序。
- 添加面向用户的状态消息。
- 添加基本快捷键：
  - `F7` 汇编
  - `F5` 运行
  - `Shift+F5` 暂停
  - `F10` 单步
- 在干净的 Windows 机器或虚拟机上验证 Release 可执行文件。
- 决定如何分发或嵌入 `config.json`。

交付物：

- 可用于演示的 Release 构建。

## 12. 建议演示程序

使用一个能够明显改变寄存器并使用 TRAP 输出的小程序：

```asm
.orig x3000
        and r0, r0, #0
        add r0, r0, #5
        add r1, r0, #3
        lea r0, msg
        puts
        halt
msg     .stringz "LC-3 GUI OK\n"
.end
```

预期演示流程：

1. 打开示例 `.asm`。
2. 汇编。
3. 加载。
4. 单步执行数次，展示 PC/寄存器变化。
5. 运行到 `HALT`。
6. 展示 TRAP 输出。
7. 在 `x3003` 设置断点，重置，运行，并展示程序在断点处暂停。

## 13. 风险列表

- 当前源文件是较大的单一翻译单元，可能让 GUI 集成变得不方便。
- 如果可执行文件位置发生变化，`config.json` 加载可能失败，除非路径处理得当。
- GUI 文本编码必须尽早修复，避免中文乱码。
- 长时间模拟必须使用定时器，否则 FLTK 会冻结。
- 当预设输入缓冲区为空时，TRAP 输入行为必须是确定的。
- 应在最终展示前尽早验证 Release 静态链接。
