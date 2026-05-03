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

## 验证结果

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
- UI 结构目前集中在 `src/main_window.cpp` 单个大文件中，和规格建议的 `src/ui/*` 拆分不同；功能上可接受，但后续维护成本较高。
- 还未在一台干净 Windows 机器或虚拟机上验证 Release 可执行文件是否真正无需额外运行时。

## 建议下一步

1. 按 `dev_spec.md` 第 12 节完整跑一遍课堂演示流程，并记录任何 UI 或状态刷新问题。
2. 决定 Release 发布形态：是否切换为 Windows 子系统、是否复制或忽略 `config.json`。
3. 在干净 Windows 环境验证 `build\x64\Release\wlt_helper_cpp.exe` 可直接运行。
4. 如还有时间，将 `main_window.cpp` 中的表格、文件操作、TRAP 面板等拆分为更小模块，降低最终答辩时解释复杂度。
