#include "command_line.h"

#include "self_test.h"
#include "src/gui/main_window.h"
#include "LC3/lc3_gui_adapter.h"
#include "src/gui/io/file_utils.h"

#include <FL/Fl.H>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace app {
namespace {

constexpr std::uint64_t kCliMaxSteps = 1'000'000;

void printUsage(std::ostream& out, const char* executable) {
    const char* name = executable ? executable : "lc3_studio";
    out << "Usage:\n"
        << "  " << name << "                 Start GUI\n"
        << "  " << name << " -a xxx.asm       Assemble xxx.asm to xxx.bin\n"
        << "  " << name << " -r xxx.bin       Run machine code and print registers\n"
        << "  " << name << " --self-test      Run built-in self test\n";
}

std::filesystem::path machineCodeOutputPath(std::filesystem::path source_path) {
    source_path.replace_extension(".bin");
    return source_path;
}

void printRegisterDump(const lc3::RegisterView& registers) {
    for (int i = 0; i < 8; i++) {
        std::cout << "R" << i << "=" << lc3::formatHexWord(registers.r[i]) << "\n";
    }
    std::cout << "PC=" << lc3::formatHexWord(registers.pc) << "\n"
              << "IR=" << lc3::formatHexWord(registers.ir) << "\n"
              << "CC=" << registers.cc << "\n"
              << "HALTED=" << (registers.halted ? "true" : "false") << "\n"
              << "STEPS=" << registers.executed_instructions << "\n";
}

int runAssembleCommand(const char* asm_path_text) {
    try {
        std::filesystem::path asm_path = ui::utf8Path(asm_path_text);
        std::string source = ui::readFile(asm_path);

        lc3::AssemblerService assembler;
        lc3::AssembleResult assembled = assembler.assembleSource(source);
        if (!assembled.ok) {
            std::cerr << "Assembly failed: " << assembled.error_message << "\n";
            return 1;
        }

        std::filesystem::path output_path = machineCodeOutputPath(asm_path);
        ui::writeFile(output_path, assembled.machine_code);
        std::cout << "Assembled " << asm_path.string()
                  << " -> " << output_path.string()
                  << " (" << assembled.words.size() << " words)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Assembly command failed: " << e.what() << "\n";
        return 1;
    }
}

int runMachineCodeCommand(const char* bin_path_text) {
    try {
        std::filesystem::path bin_path = ui::utf8Path(bin_path_text);
        std::string machine_code = ui::readFile(bin_path);

        lc3::SimulatorService simulator;
        lc3::OperationResult loaded = simulator.loadMachineCode(machine_code);
        if (!loaded.ok) {
            std::cerr << "Load failed: " << loaded.message << "\n";
            return 1;
        }
        if (simulator.registers().loaded_words == 0) {
            std::cerr << "Load failed: machine code is empty\n";
            return 1;
        }

        lc3::OperationResult running = simulator.setRunning(true);
        if (!running.ok) {
            std::cerr << "Run failed: " << running.message << "\n";
            return 1;
        }

        std::uint64_t cli_steps = 0;
        while (simulator.isRunning()) {
            if (cli_steps >= kCliMaxSteps) {
                simulator.setRunning(false);
                std::cerr << "Run failed: max steps exceeded (" << kCliMaxSteps
                          << "); possible infinite loop\n";
                return 1;
            }

            lc3::RunStepResult step = simulator.stepForRun();
            if (!step.ok) {
                std::cerr << "Run failed: " << step.message << "\n";
                return 1;
            }
            if (step.executed) {
                cli_steps++;
            }
        }

        printRegisterDump(simulator.registers());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Run command failed: " << e.what() << "\n";
        return 1;
    }
}

void closeWindowForGuiCloseTest(void* data) {
    static_cast<MainWindow*>(data)->hide();
}

int runGuiCloseTest(int, char** argv) {
    Fl::scheme("gtk+");

    MainWindow window(1180, 760);
    Fl::add_timeout(0.05, closeWindowForGuiCloseTest, &window);
    int fltk_argc = 1;
    window.show(fltk_argc, argv);
    return Fl::run();
}

} // namespace

int run(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest();
    }
    if (argc > 1 && std::string(argv[1]) == "--gui-close-test") {
        return runGuiCloseTest(argc, argv);
    }
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printUsage(std::cout, argv[0]);
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "-a") {
        if (argc != 3) {
            printUsage(std::cerr, argv[0]);
            return 1;
        }
        return runAssembleCommand(argv[2]);
    }
    if (argc > 1 && std::string(argv[1]) == "-r") {
        if (argc != 3) {
            printUsage(std::cerr, argv[0]);
            return 1;
        }
        return runMachineCodeCommand(argv[2]);
    }
    if (argc > 1 && argv[1][0] == '-') {
        printUsage(std::cerr, argv[0]);
        return 1;
    }

    Fl::scheme("gtk+");

    MainWindow window(1180, 760);
    window.show(argc, argv);
    return Fl::run();
}

} // namespace app
