#include "main_window.h"

#include <FL/Fl.H>

#include "embedded_examples.h"
#include "LC3/lc3_gui_adapter.h"

#include <cstddef>
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

std::string sourceMetadataSelfTestSource() {
    return ".orig x3000\n"
           "VALUE   .fill x1234\n"
           "TEXT    .stringz \"A\"\n"
           "        halt\n"
           ".end\n";
}

int runSelfTest() {
    lc3::AssemblerService assembler;

    if (embedded_examples::exampleCount() < 2) {
        std::cerr << "Embedded examples self test failed: expected multiple examples, got "
                  << embedded_examples::exampleCount() << "\n";
        return 1;
    }

    for (std::size_t i = 0; i < embedded_examples::exampleCount(); i++) {
        const embedded_examples::AssemblyExample* example = embedded_examples::exampleAt(i);
        if (!example || !example->source || std::string(example->source).empty()) {
            std::cerr << "Embedded examples self test failed: empty example at index "
                      << i << "\n";
            return 1;
        }
        lc3::AssembleResult example_assembled = assembler.assembleSource(example->source);
        if (!example_assembled.ok) {
            std::cerr << "Embedded example failed to assemble: "
                      << (example->title ? example->title : example->id)
                      << ": " << example_assembled.error_message << "\n";
            return 1;
        }
    }

    std::cout << "Embedded examples self test OK: count="
              << embedded_examples::exampleCount() << "\n";

    lc3::AssembleResult assembled = assembler.assembleSource(selfTestSource());
    if (!assembled.ok) {
        std::cerr << "Assembly failed: " << assembled.error_message << "\n";
        return 1;
    }

    lc3::AssembleResult metadata_assembled = assembler.assembleSource(sourceMetadataSelfTestSource());
    if (!metadata_assembled.ok || metadata_assembled.words.size() != metadata_assembled.word_sources.size() ||
        metadata_assembled.word_sources.size() != 4 ||
        metadata_assembled.word_sources[0].find(".fill") == std::string::npos ||
        metadata_assembled.word_sources[1].find(".stringz") == std::string::npos ||
        metadata_assembled.word_sources[2].find(".stringz") == std::string::npos ||
        metadata_assembled.word_sources[3].find("halt") == std::string::npos) {
        std::cerr << "Source metadata self test failed\n";
        return 1;
    }

    std::cout << "Source metadata self test OK: words="
              << metadata_assembled.word_sources.size() << "\n";

    lc3::SimulatorService simulator;
    lc3::OperationResult loaded = simulator.loadMachineCode(assembled.machine_code);
    if (!loaded.ok) {
        std::cerr << "Load failed: " << loaded.message << "\n";
        return 1;
    }
    if (simulator.registers().executed_instructions != 0) {
        std::cerr << "Instruction counter self test failed: expected 0 after load\n";
        return 1;
    }

    lc3::OperationResult stepped = simulator.stepOnce();
    if (!stepped.ok) {
        std::cerr << "Step failed: " << stepped.message << "\n";
        return 1;
    }

    lc3::RegisterView registers = simulator.registers();
    if (registers.executed_instructions != 1) {
        std::cerr << "Instruction counter self test failed: expected 1 after step, got "
                  << registers.executed_instructions << "\n";
        return 1;
    }
    std::cout << "Self test OK: "
              << "words=" << assembled.words.size()
              << " PC=" << lc3::formatHexWord(registers.pc)
              << " R0=" << lc3::formatHexWord(registers.r[0])
              << " CC=" << registers.cc
              << " STEPS=" << registers.executed_instructions << "\n";

    lc3::OperationResult reset_counter = simulator.resetProgram();
    registers = simulator.registers();
    if (!reset_counter.ok || registers.executed_instructions != 0 || registers.pc != 0x3000) {
        std::cerr << "Instruction counter reset self test failed: PC="
                  << lc3::formatHexWord(registers.pc)
                  << " steps=" << registers.executed_instructions << "\n";
        return 1;
    }

    std::cout << "Instruction counter reset self test OK\n";

    lc3::OperationResult edit_register = simulator.setRegisterValue(0, 0x8001);
    lc3::OperationResult edit_pc = simulator.setPC(0x3002);
    lc3::OperationResult edit_ir = simulator.setIR(0x1234);
    lc3::OperationResult edit_cc = simulator.setConditionCode("p");
    lc3::OperationResult edit_memory = simulator.setMemoryValue(0x3002, 0xF025);
    lc3::OperationResult edit_halted = simulator.setHalted(false);
    registers = simulator.registers();
    auto edited_memory = simulator.memoryWindow(0x3002, 0, 0);
    if (!edit_register.ok || !edit_pc.ok || !edit_ir.ok || !edit_cc.ok ||
        !edit_memory.ok || !edit_halted.ok || registers.r[0] != 0x8001 ||
        registers.pc != 0x3002 || registers.ir != 0x1234 || registers.cc != "p" ||
        edited_memory.empty() || edited_memory.front().value != 0xF025) {
        std::cerr << "Manual state edit self test failed\n";
        return 1;
    }

    std::cout << "Manual state edit self test OK: PC=" << lc3::formatHexWord(registers.pc)
              << " R0=" << lc3::formatHexWord(registers.r[0])
              << " IR=" << lc3::formatHexWord(registers.ir)
              << " CC=" << registers.cc << "\n";

    int parsed_value = 0;
    if (!lc3::parseAddress("x123F", parsed_value) || parsed_value != 0x123F ||
        !lc3::parseAddress("d12345", parsed_value) || parsed_value != 12345 ||
        !lc3::parseAddress("12345", parsed_value) || parsed_value != 12345 ||
        !lc3::parseAddress("b1000_0010 1011 1100", parsed_value) || parsed_value != 0x82BC) {
        std::cerr << "Numeric parser self test failed\n";
        return 1;
    }

    std::cout << "Numeric parser self test OK\n";

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
    if (!step_over_breakpoint.ok || breakpoint_registers.pc != 0x3002 ||
        breakpoint_registers.executed_instructions != 2) {
        std::cerr << "Breakpoint step-over failed: PC="
                  << lc3::formatHexWord(breakpoint_registers.pc)
                  << " steps=" << breakpoint_registers.executed_instructions << "\n";
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

    lc3::RegisterView trap_registers = trap_simulator.registers();
    if (!trap_simulator.isHalted() || trap_simulator.trapOutputBuffer() != "A" ||
        trap_registers.executed_instructions != 3) {
        std::cerr << "TRAP self test failed: output=\"" << trap_simulator.trapOutputBuffer()
                  << "\" halted=" << (trap_simulator.isHalted() ? "true" : "false")
                  << " steps=" << trap_registers.executed_instructions << "\n";
        return 1;
    }

    std::cout << "TRAP self test OK: output=\"" << trap_simulator.trapOutputBuffer()
              << "\" steps=" << trap_registers.executed_instructions << "\n";
    return 0;
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

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return runSelfTest();
    }
    if (argc > 1 && std::string(argv[1]) == "--gui-close-test") {
        return runGuiCloseTest(argc, argv);
    }

    Fl::scheme("gtk+");

    MainWindow window(1180, 760);
    window.show(argc, argv);
    return Fl::run();
}
