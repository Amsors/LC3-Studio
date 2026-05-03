# LC-3 GUI 开发状态简报

更新时间：2026-05-03

## 总体结论

当前项目已经从 `dev_spec.md` 的阶段 0 推进到接近阶段 8。GUI、LC-3 核心适配、汇编/加载、单步、运行/暂停、地址断点、TRAP 输入输出、文件打开保存和示例程序均已有实现，已经具备课堂演示的主体功能。

## 已完成情况

- 阶段 0 基线清理：已完成。Visual Studio 工程使用 C++17、`/utf-8`，Debug 使用 `/MTd`，Release 使用 `/MT`，FLTK include/lib 路径已配置。
- 阶段 1 核心集成：已完成。`LC3/lc3_gui_adapter.h` 暴露 `AssemblerService` 和 `SimulatorService`，`assembler.cpp`、`runtime.cpp` 通过 `LC3_AS_LIBRARY` 方式作为库代码接入 GUI。
- 阶段 2 ASM 编辑器和汇编输出：已完成。界面包含 `Fl_Text_Editor` 源码编辑器、汇编按钮、机器码显示、日志显示、打开/保存 `.asm` 文件。
- 阶段 3 加载并检查机器状态：已完成。支持 Assemble+Load，显示 R0-R7、PC、IR、CC、RUNNING、HALTED，支持内存窗口和地址跳转。
- 阶段 4 单步调试：已完成。Step 后刷新寄存器、内存、PC 高亮和状态栏，HALT 后进入停止状态。
- 阶段 5 TRAP 输入输出：已完成。已有预设输入缓冲区、输出显示、清空输出、剩余输入计数，并接入 `SetReadCharHandler`/`SetWriteCharHandler`。
- 阶段 6 运行/暂停：已完成。使用 `Fl::add_timeout()`/`Fl::repeat_timeout()` 分批执行，避免 GUI 长时间阻塞。
- 阶段 7 地址断点：已完成。支持地址输入添加/移除断点、清空断点、双击内存行切换断点，Run 会在断点地址执行前暂停，Step 可越过断点。
- 阶段 8 展示准备：大部分完成。已有示例程序、状态消息和快捷键；Release 产物已存在。
- ASM Source 编辑器代码高亮：已完成。`Fl_Text_Editor` 现在使用独立 style buffer，对 LC-3 指令/伪指令、寄存器、立即数/字符串、标签和注释分别使用不同颜色显示，并会在用户编辑、打开文件和载入 demo 时自动刷新。

## 验证结果

已运行 Release 构建：

```text
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" wlt_helper_cpp.sln /p:Configuration=Release /p:Platform=x64 /m
```

结果通过：

- 0 个警告，0 个错误。

已运行 Release 自检：

```text
build\x64\Release\wlt_helper_cpp.exe --self-test
```

结果通过：

- 汇编、加载、单步通过。
- 断点运行到 `x3001` 后暂停，单步可继续到 `x3002`。
- TRAP 输入输出通过，输入 `"A"` 后输出 `"A"`。

也从 `build\x64\Release` 目录直接运行 `.\wlt_helper_cpp.exe --self-test`，结果同样通过。

## 当前主要风险

- 还没有实际检查完整 GUI 交互流程，例如打开窗口后手工执行“打开示例、汇编、加载、单步、运行到 HALT、设置断点、重置再运行”的课堂演示路径。
- `config.json` 虽然已有内置默认配置兜底，自检也能在 Release 目录直接通过，但发布策略仍需明确：是否完全依赖内置配置，还是同时随包带上 `LC3/config.json`。
- 工程链接子系统仍是 `Console`，展示时会额外出现控制台窗口；如果希望像普通桌面程序一样启动，应考虑 Release 改为 `Windows` 子系统并保留调试自检方案。
- UI 已开始按 `src/ui/*` 拆分；`MainWindow` 仍保留布局和业务协调逻辑，后续如继续维护，可进一步拆分控制器流程和 TRAP 面板。
- 还未在一台干净 Windows 机器或虚拟机上验证 Release 可执行文件是否真正无需额外运行时。

## 建议下一步

1. 按 `dev_spec.md` 第 12 节完整跑一遍课堂演示流程，并记录任何 UI 或状态刷新问题。
2. 决定 Release 发布形态：是否切换为 Windows 子系统、是否复制或忽略 `config.json`。
3. 在干净 Windows 环境验证 `build\x64\Release\wlt_helper_cpp.exe` 可直接运行。
4. 如还有时间，继续将 `main_window.cpp` 中的运行控制、TRAP 面板和文件命令拆分为更小模块，降低最终答辩时解释复杂度。

## 2026-05-03 新增：手动修改状态机状态

- 新增 Load 后/单步暂停时手动修改 LC-3 状态的能力。
- 寄存器表支持双击编辑 R0-R7、PC、IR、CC 和 HALTED；RUNNING 仍由 Run/Pause 控制，避免和 FLTK 定时器状态冲突。
- 内存表支持双击非 Flag 列修改该地址的 16 位内容；双击 Flag 列仍保留原有切换断点行为。
- GUI 顶部新增提示文本，检测到手动修改后显示“状态机状态被人为修改”，并在日志中记录修改项。
- `SimulatorService` 新增 `setMemoryValue`、`setRegisterValue`、`setPC`、`setIR`、`setConditionCode`、`setHalted` 接口；底层 `StateMachine` 增加对应写入入口。
- `--self-test` 已扩展覆盖手动修改内存、寄存器、PC、IR、CC 和 HALTED 的路径。

## 2026-05-03 优化：表格内联编辑与数字输入格式

- 将状态编辑从弹窗输入改为表格内联输入：双击寄存器值或内存值单元格后，在原单元格位置显示输入框，按 Enter 确认。
- 合法输入直接提交并刷新视图；非法输入才弹出提示框，并保留输入框方便继续修改。
- 内存表中双击 `Flag` 列仍用于切换断点；双击 `Hex` 或 `Binary` 列用于修改该地址的内存内容。
- 数字解析支持 `x123F` 十六进制、`d12345` 十进制、`b1000_0010 1011 1100` 二进制；无 `x`/`d`/`b` 前缀时默认按十进制处理。
- 数字输入中的空格和下划线会被忽略，便于分组输入。

## 2026-05-03 修复：内联编辑单元格定位失败

- 修复双击内存/寄存器表格时总是提示 `Memory cell is not visible` 或 `Register cell is not visible` 的问题。
- `RegisterTable` 和 `MemoryTable` 现在会在绘制可见单元格时缓存单元格坐标；开始内联编辑时优先使用 FLTK `find_cell`，失败时回退到绘制缓存定位输入框。
- Release 构建通过，`build\x64\Release\wlt_helper_cpp.exe --self-test` 通过。

## 2026-05-03 重构：GUI 代码模块化拆分

- 新增 `src/ui/asm_highlighter.*`，将 LC-3 汇编源码高亮规则、token 识别和 style buffer 文本生成从 `main_window.cpp` 中拆出。
- 新增 `src/ui/register_table.*` 和 `src/ui/memory_table.*`，将寄存器表、内存表的绘制、列宽调整、可编辑行判断和可见单元格定位逻辑独立成控件模块。
- 新增 `src/ui/file_utils.*`，集中处理 UTF-8 文件路径转换、文件读写和布尔文本解析。
- 保留 `src/main_window.cpp` 负责主窗口布局、FLTK 回调和 LC-3 服务协调；文件体积已从约 63KB 降到约 45KB，后续维护边界更清晰。
- 更新 Visual Studio 工程文件，加入 `src\ui\*.cpp` 和 `src\ui\*.h`，并在 filters 中增加 UI 分组，方便 VS2022 中浏览。
- Release 构建通过，`build\x64\Release\wlt_helper_cpp.exe --self-test` 通过。
