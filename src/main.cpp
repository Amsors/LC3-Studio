#include "main_window.h"

#include <FL/Fl.H>

#include "LC3/lc3_gui_adapter.h"

#include <iostream>
#include <string>

namespace {

std::string selfTestSource() {
    return ".orig x3000\n"
           "        and r0, r0, #0\n"
           "        add r0, r0, #5\n"
           "        halt\n"
           ".end\n";
}

int runSelfTest() {
    lc3::AssemblerService assembler;
    lc3::AssembleResult assembled = assembler.assembleSource(selfTestSource());
    if (!assembled.ok) {
        std::cerr << "Assembly failed: " << assembled.error_message << "\n";
        return 1;
    }

    lc3::SimulatorService simulator;
    lc3::OperationResult loaded = simulator.loadMachineCode(assembled.machine_code);
    if (!loaded.ok) {
        std::cerr << "Load failed: " << loaded.message << "\n";
        return 1;
    }

    lc3::OperationResult stepped = simulator.stepOnce();
    if (!stepped.ok) {
        std::cerr << "Step failed: " << stepped.message << "\n";
        return 1;
    }

    lc3::RegisterView registers = simulator.registers();
    std::cout << "Self test OK: "
              << "words=" << assembled.words.size()
              << " PC=" << lc3::formatHexWord(registers.pc)
              << " R0=" << lc3::formatHexWord(registers.r[0])
              << " CC=" << registers.cc << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest();
    }

    Fl::scheme("gtk+");

    MainWindow window(1100, 720);
    window.show(argc, argv);
    return Fl::run();
}
