#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lc3 {

struct OperationResult {
    bool ok = false;
    std::string message;
};

struct AssembleResult {
    bool ok = false;
    std::string machine_code;
    std::vector<std::string> words;
    std::vector<std::string> word_sources;
    std::string error_message;
    int error_line = 0;
};

class AssemblerService {
public:
    AssembleResult assembleSource(const std::string& source) const;
};

struct RegisterView {
    int r[8] = {};
    int pc = 0;
    int ir = 0;
    std::string cc;
    bool halted = false;
    bool running = false;
    int loaded_start = 0x3000;
    int loaded_words = 0;
    std::uint64_t executed_instructions = 0;
};

struct MemoryRow {
    int address = 0;
    int value = 0;
    bool is_pc = false;
    bool has_breakpoint = false;
    std::string source;
};

struct RunStepResult {
    bool ok = false;
    bool executed = false;
    bool stopped = false;
    std::string message;
};

class SimulatorService {
public:
    SimulatorService();
    ~SimulatorService();

    SimulatorService(SimulatorService&&) noexcept;
    SimulatorService& operator=(SimulatorService&&) noexcept;

    SimulatorService(const SimulatorService&) = delete;
    SimulatorService& operator=(const SimulatorService&) = delete;

    OperationResult loadMachineCode(const std::string& machine_code);
    OperationResult resetProgram();
    void clearMachine();

    OperationResult stepOnce();
    RunStepResult stepForRun();

    OperationResult setRunning(bool value);
    bool isRunning() const;
    bool isHalted() const;

    void setBreakpoint(int address, bool enabled);
    bool hasBreakpoint(int address) const;
    void clearBreakpoints();

    RegisterView registers() const;
    std::vector<MemoryRow> memoryWindow(int center_address, int before, int after) const;

    OperationResult setMemoryValue(int address, int value);
    OperationResult setRegisterValue(int index, int value);
    OperationResult setPC(int value);
    OperationResult setIR(int value);
    OperationResult setConditionCode(const std::string& cc);
    OperationResult setHalted(bool value);

    void setTrapInputBuffer(const std::string& text);
    std::string trapInputRemainder() const;
    std::string trapOutputBuffer() const;
    void clearTrapOutputBuffer();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool parseAddress(const std::string& text, int& address);
std::string formatHexWord(int value);
std::string formatBinaryWord(int value);

} // namespace lc3
