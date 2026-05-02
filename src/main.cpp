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

std::string trapEchoSelfTestSource() {
    return ".orig x3000\n"
           "        getc\n"
           "        out\n"
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

    lc3::SimulatorService breakpoint_simulator;
    lc3::OperationResult breakpoint_loaded = breakpoint_simulator.loadMachineCode(assembled.machine_code);
    if (!breakpoint_loaded.ok) {
        std::cerr << "Breakpoint load failed: " << breakpoint_loaded.message << "\n";
        return 1;
    }
    breakpoint_simulator.setBreakpoint(0x3001, true);
    lc3::OperationResult breakpoint_running = breakpoint_simulator.setRunning(true);
    if (!breakpoint_running.ok) {
        std::cerr << "Breakpoint run failed: " << breakpoint_running.message << "\n";
        return 1;
    }
    lc3::RunStepResult first_run_step = breakpoint_simulator.stepForRun();
    lc3::RunStepResult breakpoint_step = breakpoint_simulator.stepForRun();
    lc3::RegisterView breakpoint_registers = breakpoint_simulator.registers();
    if (!first_run_step.ok || first_run_step.stopped ||
        !breakpoint_step.ok || !breakpoint_step.stopped ||
        breakpoint_registers.pc != 0x3001 || breakpoint_simulator.isRunning()) {
        std::cerr << "Breakpoint self test failed: PC="
                  << lc3::formatHexWord(breakpoint_registers.pc)
                  << " running=" << (breakpoint_simulator.isRunning() ? "true" : "false")
                  << " stopped=" << (breakpoint_step.stopped ? "true" : "false") << "\n";
        return 1;
    }
    lc3::OperationResult step_over_breakpoint = breakpoint_simulator.stepOnce();
    breakpoint_registers = breakpoint_simulator.registers();
    if (!step_over_breakpoint.ok || breakpoint_registers.pc != 0x3002) {
        std::cerr << "Breakpoint step-over failed: PC="
                  << lc3::formatHexWord(breakpoint_registers.pc) << "\n";
        return 1;
    }

    std::cout << "Breakpoint self test OK: stopped at x3001, step reached "
              << lc3::formatHexWord(breakpoint_registers.pc) << "\n";

    lc3::AssembleResult trap_assembled = assembler.assembleSource(trapEchoSelfTestSource());
    if (!trap_assembled.ok) {
        std::cerr << "TRAP assembly failed: " << trap_assembled.error_message << "\n";
        return 1;
    }

    lc3::SimulatorService trap_simulator;
    trap_simulator.setTrapInputBuffer("A");
    lc3::OperationResult trap_loaded = trap_simulator.loadMachineCode(trap_assembled.machine_code);
    if (!trap_loaded.ok) {
        std::cerr << "TRAP load failed: " << trap_loaded.message << "\n";
        return 1;
    }

    lc3::OperationResult running = trap_simulator.setRunning(true);
    if (!running.ok) {
        std::cerr << "TRAP run failed: " << running.message << "\n";
        return 1;
    }

    for (int i = 0; i < 16 && trap_simulator.isRunning(); i++) {
        lc3::RunStepResult step = trap_simulator.stepForRun();
        if (!step.ok) {
            std::cerr << "TRAP run step failed: " << step.message << "\n";
            return 1;
        }
    }

    if (!trap_simulator.isHalted() || trap_simulator.trapOutputBuffer() != "A") {
        std::cerr << "TRAP self test failed: output=\"" << trap_simulator.trapOutputBuffer()
                  << "\" halted=" << (trap_simulator.isHalted() ? "true" : "false") << "\n";
        return 1;
    }

    std::cout << "TRAP self test OK: output=\"" << trap_simulator.trapOutputBuffer() << "\"\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest();
    }

    Fl::scheme("gtk+");

    MainWindow window(1180, 760);
    window.show(argc, argv);
    return Fl::run();
}
