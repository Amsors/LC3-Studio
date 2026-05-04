#define LC3_AS_LIBRARY
#include "runtime.cpp"

#include "lc3_gui_adapter.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>

namespace lc3 {

namespace {

std::string trim(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

std::string lowercaseAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

} // namespace

struct SimulatorService::Impl {
    StateMachine machine;
    std::set<int> breakpoints;
    std::string loaded_machine_code;
    std::string trap_input;
    std::size_t trap_input_pos = 0;
    std::string trap_output;

    Impl() {
        installTrapHandlers();
    }

    void installTrapHandlers() {
        machine.SetReadCharHandler([this]() -> int {
            if (trap_input_pos >= trap_input.size()) {
                return 0;
            }
            return static_cast<unsigned char>(trap_input[trap_input_pos++]);
        });
        machine.SetWriteCharHandler([this](unsigned char c) {
            trap_output.push_back(static_cast<char>(c));
        });
    }
};

SimulatorService::SimulatorService() : impl_(std::make_unique<Impl>()) {}

SimulatorService::~SimulatorService() = default;

SimulatorService::SimulatorService(SimulatorService&&) noexcept = default;

SimulatorService& SimulatorService::operator=(SimulatorService&&) noexcept = default;

OperationResult SimulatorService::loadMachineCode(const std::string& machine_code) {
    try {
        impl_->machine.LoadCodeFromString(machine_code);
        impl_->installTrapHandlers();
        impl_->loaded_machine_code = machine_code;
        impl_->trap_input_pos = 0;
        return { true, "Machine code loaded" };
    } catch (const std::exception& e) {
        return { false, e.what() };
    }
}

OperationResult SimulatorService::resetProgram() {
    if (impl_->loaded_machine_code.empty()) {
        clearMachine();
        return { true, "Machine cleared" };
    }
    std::string code = impl_->loaded_machine_code;
    return loadMachineCode(code);
}

void SimulatorService::clearMachine() {
    impl_->machine.Reset();
    impl_->installTrapHandlers();
    impl_->loaded_machine_code.clear();
    impl_->trap_input_pos = 0;
}

OperationResult SimulatorService::stepOnce() {
    try {
        bool can_continue = impl_->machine.StepOnce();
        return { true, can_continue ? "Step executed" : "Machine halted" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

RunStepResult SimulatorService::stepForRun() {
    try {
        int pc = impl_->machine.ReadPCValue();
        if (hasBreakpoint(pc)) {
            impl_->machine.SetRunning(false);
            return { true, false, true, "Paused at breakpoint " + formatHexWord(pc) };
        }

        bool can_continue = impl_->machine.StepOnce();
        if (!can_continue) {
            impl_->machine.SetRunning(false);
            auto snapshot = impl_->machine.GetSnapshot();
            return { true,
                     true,
                     true,
                     "Machine halted; steps=" +
                         std::to_string(snapshot.executed_instructions) };
        }
        return { true, true, false, "Step executed" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, false, true, e.what() };
    }
}

OperationResult SimulatorService::setRunning(bool value) {
    try {
        impl_->machine.SetRunning(value);
        return { true, value ? "Running" : "Paused" };
    } catch (const std::exception& e) {
        return { false, e.what() };
    }
}

bool SimulatorService::isRunning() const {
    return impl_->machine.IsRunning();
}

bool SimulatorService::isHalted() const {
    return impl_->machine.IsHalted();
}

void SimulatorService::setBreakpoint(int address, bool enabled) {
    address &= 0xFFFF;
    if (enabled) {
        impl_->breakpoints.insert(address);
    } else {
        impl_->breakpoints.erase(address);
    }
}

bool SimulatorService::hasBreakpoint(int address) const {
    return impl_->breakpoints.find(address & 0xFFFF) != impl_->breakpoints.end();
}

void SimulatorService::clearBreakpoints() {
    impl_->breakpoints.clear();
}

RegisterView SimulatorService::registers() const {
    auto snapshot = impl_->machine.GetSnapshot();
    RegisterView view;
    for (int i = 0; i < 8; i++) {
        view.r[i] = snapshot.registers[i];
    }
    view.pc = snapshot.pc;
    view.ir = snapshot.ir;
    view.cc = StateMachine::CCToString(snapshot.cc);
    view.halted = snapshot.halted;
    view.running = snapshot.running;
    view.loaded_start = snapshot.loaded_start;
    view.loaded_words = snapshot.loaded_words;
    view.executed_instructions = snapshot.executed_instructions;
    return view;
}

std::vector<MemoryRow> SimulatorService::memoryWindow(int center_address, int before, int after) const {
    before = std::max(0, before);
    after = std::max(0, after);
    center_address &= 0xFFFF;

    int start = std::max(0, center_address - before);
    int end = std::min(StateMachine::MEMORY_SIZE - 1, center_address + after);
    std::vector<int> values = impl_->machine.ReadMemoryRange(start, end - start + 1);

    int pc = impl_->machine.ReadPCValue();
    std::vector<MemoryRow> rows;
    rows.reserve(values.size());
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
        int address = start + i;
        rows.push_back({ address,
                         values[static_cast<std::size_t>(i)],
                         address == pc,
                         hasBreakpoint(address),
                         "" });
    }
    return rows;
}

OperationResult SimulatorService::setMemoryValue(int address, int value) {
    try {
        impl_->machine.WriteSingleMem(address & 0xFFFF, value & 0xFFFF);
        return { true, "Memory updated" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

OperationResult SimulatorService::setRegisterValue(int index, int value) {
    try {
        impl_->machine.SetRegisterValue(index, value & 0xFFFF);
        return { true, "Register updated" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

OperationResult SimulatorService::setPC(int value) {
    try {
        impl_->machine.SetPC(value & 0xFFFF);
        return { true, "PC updated" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

OperationResult SimulatorService::setIR(int value) {
    try {
        impl_->machine.SetIR(value & 0xFFFF);
        return { true, "IR updated" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

OperationResult SimulatorService::setConditionCode(const std::string& cc) {
    std::string s = lowercaseAscii(trim(cc));
    try {
        if (s == "n" || s == "neg" || s == "negative") {
            impl_->machine.SetConditionCode(StateMachine::NEG);
        } else if (s == "z" || s == "zero") {
            impl_->machine.SetConditionCode(StateMachine::ZERO);
        } else if (s == "p" || s == "pos" || s == "positive") {
            impl_->machine.SetConditionCode(StateMachine::POS);
        } else {
            return { false, "Condition code must be n, z, or p" };
        }
        return { true, "Condition code updated" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

OperationResult SimulatorService::setHalted(bool value) {
    try {
        impl_->machine.SetHalted(value);
        return { true, "HALTED updated" };
    } catch (const std::exception& e) {
        impl_->machine.SetRunning(false);
        return { false, e.what() };
    }
}

void SimulatorService::setTrapInputBuffer(const std::string& text) {
    impl_->trap_input = text;
    impl_->trap_input_pos = 0;
}

std::string SimulatorService::trapInputRemainder() const {
    if (impl_->trap_input_pos >= impl_->trap_input.size()) {
        return {};
    }
    return impl_->trap_input.substr(impl_->trap_input_pos);
}

std::string SimulatorService::trapOutputBuffer() const {
    return impl_->trap_output;
}

void SimulatorService::clearTrapOutputBuffer() {
    impl_->trap_output.clear();
}

bool parseAddress(const std::string& text, int& address) {
    std::string s;
    for (char c : trim(text)) {
        if (c != '_' && !std::isspace(static_cast<unsigned char>(c))) {
            s.push_back(c);
        }
    }
    if (s.empty()) return false;

    int base = 10;
    std::size_t pos = 0;
    bool negative = false;
    if (s[pos] == '-' || s[pos] == '+') {
        negative = s[pos] == '-';
        pos++;
    }
    if (pos >= s.size()) return false;

    if (s[pos] == '#') {
        base = 10;
        pos++;
    } else if (s[pos] == 'd' || s[pos] == 'D') {
        base = 10;
        pos++;
    } else if (s[pos] == 'b' || s[pos] == 'B') {
        base = 2;
        pos++;
    } else if (s[pos] == 'x' || s[pos] == 'X') {
        base = 16;
        pos++;
    } else if (pos + 1 < s.size() && s[pos] == '0' && (s[pos + 1] == 'x' || s[pos + 1] == 'X')) {
        base = 16;
        pos += 2;
    }
    if (pos >= s.size()) return false;

    int value = 0;
    for (; pos < s.size(); pos++) {
        char c = s[pos];
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        if (digit >= base) return false;
        value = value * base + digit;
        if (value > 0xFFFF) return false;
    }

    if (negative) return false;
    address = value;
    return true;
}

std::string formatHexWord(int value) {
    std::ostringstream out;
    out << 'x' << std::uppercase << std::hex << std::setfill('0') << std::setw(4)
        << (value & 0xFFFF);
    return out.str();
}

std::string formatBinaryWord(int value) {
    std::string result;
    result.reserve(16);
    for (int bit = 15; bit >= 0; bit--) {
        result.push_back((value & (1 << bit)) ? '1' : '0');
    }
    return result;
}

} // namespace lc3
