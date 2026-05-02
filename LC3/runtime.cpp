// LC-3 runtime / state machine.
//
// GUI-facing API:
//   StateMachine s;
//   s.LoadCodeFromString(machine_code); // newline words or legacy flat 01 string
//   s.StepOnce();                       // execute exactly one instruction
//   auto state = s.GetSnapshot();        // registers, PC, IR, CC, running
//
// TRAP is intentionally implemented as high-level host I/O. RTI is not
// implemented and raises RuntimeError when executed.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const std::string& msg) : std::runtime_error(msg) {}
};

class Config {
public:
    static bool READ_FROM_FILE;
    static std::string HALT_INSTRUCTION;
    static bool TRACE;
    static int MAX_STEPS;
};

bool Config::READ_FROM_FILE = false;
std::string Config::HALT_INSTRUCTION = "1111000000100101";
bool Config::TRACE = false;
int Config::MAX_STEPS = 10'000'000;

class CodeReader {
public:
    static bool isValidLine(const std::string& line) {
        if (line.size() != 16) return false;
        for (char c : line) {
            if (c != '0' && c != '1') return false;
        }
        return true;
    }

    static std::string Trim(std::string line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto p = line.find(';');
        if (p != std::string::npos) line.resize(p);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;
        return line.substr(start);
    }

    static std::vector<std::string> ReadFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw RuntimeError("Cannot open file: " + filename);
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return ReadFromString(buffer.str());
    }

    static std::vector<std::string> ReadFromString(const std::string& source) {
        std::string compact;
        bool compact_only = true;
        for (char c : source) {
            if (c == '0' || c == '1') {
                compact += c;
            } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                continue;
            } else {
                compact_only = false;
                break;
            }
        }
        if (compact_only && !compact.empty() && compact.length() % 16 == 0) {
            std::vector<std::string> result;
            for (size_t i = 0; i < compact.length(); i += 16) {
                result.push_back(compact.substr(i, 16));
            }
            return result;
        }

        std::vector<std::string> result;
        std::istringstream iss(source);
        std::string line;
        int lineno = 0;
        while (std::getline(iss, line)) {
            lineno++;
            line = Trim(line);
            if (line.empty()) continue;
            if (!isValidLine(line)) {
                throw RuntimeError("Invalid instruction at line " +
                                   std::to_string(lineno) + ": " + line);
            }
            result.push_back(line);
        }

        if (!result.empty()) return result;

        compact.clear();
        for (char c : source) {
            if (c == '0' || c == '1') {
                compact += c;
            } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                continue;
            } else if (c == ';') {
                break;
            } else {
                throw RuntimeError("Invalid character in machine code");
            }
        }

        if (compact.empty()) return result;
        if (compact.length() % 16 != 0) {
            throw RuntimeError("Machine code length is not a multiple of 16");
        }
        for (size_t i = 0; i < compact.length(); i += 16) {
            result.push_back(compact.substr(i, 16));
        }
        return result;
    }

    static std::vector<std::string> ReadFromConsole() {
        std::vector<std::string> result;
        std::string line;
        while (std::cin >> line) {
            if (line == "stop") break;
            if (!isValidLine(line)) {
                throw RuntimeError("Invalid instruction: " + line);
            }
            result.push_back(line);
            if (line == Config::HALT_INSTRUCTION) break;
        }
        return result;
    }
};

class Utilities {
public:
    using MemCell = std::pair<std::byte, std::byte>;

    static MemCell Get16BitFromStr(const std::string& s) {
        if (s.length() != 16) {
            throw RuntimeError("Get16BitFromStr: expected 16 chars, got " +
                               std::to_string(s.length()));
        }
        unsigned char b1 = 0, b2 = 0;
        for (int i = 0; i < 8; i++) {
            char c = s[i];
            if (c != '0' && c != '1') throw RuntimeError("Get16BitFromStr: non-binary char");
            if (c == '1') b1 = static_cast<unsigned char>(b1 | (1u << (7 - i)));
        }
        for (int i = 8; i < 16; i++) {
            char c = s[i];
            if (c != '0' && c != '1') throw RuntimeError("Get16BitFromStr: non-binary char");
            if (c == '1') b2 = static_cast<unsigned char>(b2 | (1u << (15 - i)));
        }
        return { std::byte{b1}, std::byte{b2} };
    }

    static void WriteMem(MemCell& m, int data) {
        unsigned int u = static_cast<unsigned int>(data) & 0xFFFFu;
        m.first = std::byte{ static_cast<unsigned char>((u >> 8) & 0xFFu) };
        m.second = std::byte{ static_cast<unsigned char>(u & 0xFFu) };
    }

    static int ReadMem(const MemCell& m) {
        int high = std::to_integer<int>(m.first) & 0xFF;
        int low = std::to_integer<int>(m.second) & 0xFF;
        return (high << 8) | low;
    }

    static void IncreaseMem(MemCell& m, int delta) {
        WriteMem(m, ReadMem(m) + delta);
    }

    static int ReadMem(const MemCell& m, int start, int length) {
        if (length <= 0 || start < length - 1 || start > 15) {
            throw RuntimeError("ReadMem(bits): invalid range");
        }
        int word = ReadMem(m);
        int low_bit = start - length + 1;
        return (word >> low_bit) & ((1 << length) - 1);
    }

    static int SignExt(int value, int bit_length) {
        if (bit_length <= 0 || bit_length > 31) {
            throw RuntimeError("SignExt: invalid bit_length");
        }
        unsigned int mask = (1u << bit_length) - 1;
        unsigned int v = static_cast<unsigned int>(value) & mask;
        unsigned int sign_bit = 1u << (bit_length - 1);
        if (v & sign_bit) {
            v -= (1u << bit_length);
        }
        return static_cast<int>(v);
    }

    static void CopyMem(const MemCell& src, MemCell& dst) {
        dst = src;
    }

    static std::string MemToHexString(const MemCell& data) {
        std::ostringstream ss;
        ss << "x" << std::hex << std::nouppercase << std::setfill('0')
           << std::setw(2) << std::to_integer<int>(data.first)
           << std::setw(2) << std::to_integer<int>(data.second);
        return ss.str();
    }

    static int HexStringToInt(const std::string& str) {
        if (str.size() != 5 || str[0] != 'x') {
            throw RuntimeError("HexStringToInt: bad format: " + str);
        }
        int result = 0;
        for (int i = 1; i < 5; i++) {
            char c = str[i];
            int v;
            if (c >= '0' && c <= '9') v = c - '0';
            else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
            else throw RuntimeError("HexStringToInt: bad hex char");
            result = result * 16 + v;
        }
        return result;
    }

    static std::string IntToHex(int value, int width) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(width)
           << (static_cast<unsigned int>(value) & 0xFFFFFFFFu);
        return ss.str();
    }
};

class StateMachine {
public:
    using MemCell = Utilities::MemCell;

    enum CC_type { POS, ZERO, NEG };

    static constexpr int MEMORY_SIZE = 0x10000;
    static constexpr int DEFAULT_PC = 0x3000;

    struct Snapshot {
        int pc = 0;
        int ir = 0;
        int registers[8] = {};
        CC_type cc = ZERO;
        bool running = false;
        bool halted = false;
        int loaded_start = DEFAULT_PC;
        int loaded_words = 0;
    };

private:
    std::vector<MemCell> mem;
    MemCell PC;
    MemCell R[8];
    MemCell IR;
    bool running;
    bool halted;
    CC_type CC;
    int loaded_start;
    int loaded_words;

    std::function<int()> read_char_fn;
    std::function<void(unsigned char)> write_char_fn;

public:
    StateMachine()
        : mem(MEMORY_SIZE, MemCell{ std::byte{0}, std::byte{0} }),
          running(false),
          halted(false),
          CC(ZERO),
          loaded_start(DEFAULT_PC),
          loaded_words(0),
          read_char_fn(DefaultReadChar),
          write_char_fn(DefaultWriteChar) {
        Utilities::WriteMem(PC, DEFAULT_PC);
        for (auto& r : R) Utilities::WriteMem(r, 0);
        Utilities::WriteMem(IR, 0);
    }

    StateMachine(const std::string& inputPath, int length, int copy_start = DEFAULT_PC)
        : StateMachine() {
        (void)length;
        std::vector<std::string> ins;
        if (Config::READ_FROM_FILE) {
            ins = CodeReader::ReadFile(inputPath);
        } else {
            ins = CodeReader::ReadFromConsole();
        }
        LoadCode(ins, copy_start);
    }

    void LoadCode(const std::vector<std::string>& code, int start_addr = DEFAULT_PC) {
        Reset();
        if (start_addr != DEFAULT_PC) {
            throw RuntimeError("Only x3000 start address is supported");
        }
        if (start_addr + static_cast<int>(code.size()) > MEMORY_SIZE) {
            throw RuntimeError("LoadCode: code too long for memory");
        }
        for (size_t i = 0; i < code.size(); i++) {
            mem[start_addr + static_cast<int>(i)] = Utilities::Get16BitFromStr(code[i]);
        }
        Utilities::WriteMem(PC, start_addr);
        loaded_start = start_addr;
        loaded_words = static_cast<int>(code.size());
        halted = false;
        running = false;

        if (Config::TRACE) {
            std::cout << "Loaded " << code.size() << " word(s) at x"
                      << Utilities::IntToHex(start_addr, 4) << "\n";
        }
    }

    void LoadCodeFromFile(const std::string& path, int start_addr = DEFAULT_PC) {
        LoadCode(CodeReader::ReadFile(path), start_addr);
    }

    void LoadCodeFromString(const std::string& src, int start_addr = DEFAULT_PC) {
        LoadCode(CodeReader::ReadFromString(src), start_addr);
    }

    void Reset() {
        for (auto& cell : mem) {
            cell = MemCell{ std::byte{0}, std::byte{0} };
        }
        Utilities::WriteMem(PC, DEFAULT_PC);
        for (auto& r : R) Utilities::WriteMem(r, 0);
        Utilities::WriteMem(IR, 0);
        CC = ZERO;
        running = false;
        halted = false;
        loaded_start = DEFAULT_PC;
        loaded_words = 0;
    }

    MemCell ReadSingleMem(int location) const {
        CheckAddress(location, "ReadSingleMem");
        return mem[location];
    }

    int ReadSingleMemValue(int location) const {
        return Utilities::ReadMem(ReadSingleMem(location));
    }

    void WriteSingleMem(int pos, int data) {
        CheckAddress(pos, "WriteSingleMem");
        Utilities::WriteMem(mem[pos], data);
    }

    MemCell ReadRegister(int num) const {
        CheckRegister(num);
        return R[num];
    }

    int ReadRegisterValue(int num) const {
        return Utilities::ReadMem(ReadRegister(num));
    }

    void WriteRegister(int num, int data) {
        CheckRegister(num);
        Utilities::WriteMem(R[num], data);
        SetCC(R[num]);
    }

    MemCell ReadPC() const { return PC; }
    MemCell ReadIR() const { return IR; }
    int ReadPCValue() const { return Utilities::ReadMem(PC); }
    int ReadIRValue() const { return Utilities::ReadMem(IR); }
    CC_type ReadCC() const { return CC; }
    bool IsRunning() const { return running; }
    bool IsHalted() const { return halted; }
    int LoadedStart() const { return loaded_start; }
    int LoadedWordCount() const { return loaded_words; }

    void SetPC(int v) {
        CheckAddress(v & 0xFFFF, "SetPC");
        Utilities::WriteMem(PC, v);
        halted = false;
    }

    void SetRunning(bool v) {
        if (v && halted) {
            throw RuntimeError("Cannot run: machine is halted");
        }
        running = v;
    }

    Snapshot GetSnapshot() const {
        Snapshot s;
        s.pc = Utilities::ReadMem(PC);
        s.ir = Utilities::ReadMem(IR);
        for (int i = 0; i < 8; i++) {
            s.registers[i] = Utilities::ReadMem(R[i]);
        }
        s.cc = CC;
        s.running = running;
        s.halted = halted;
        s.loaded_start = loaded_start;
        s.loaded_words = loaded_words;
        return s;
    }

    std::vector<int> ReadMemoryRange(int start, int count) const {
        if (count < 0) {
            throw RuntimeError("ReadMemoryRange: negative count");
        }
        CheckAddress(start, "ReadMemoryRange");
        if (count > 0) CheckAddress(start + count - 1, "ReadMemoryRange");

        std::vector<int> result;
        result.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; i++) {
            result.push_back(Utilities::ReadMem(mem[start + i]));
        }
        return result;
    }

    void SetReadCharHandler(std::function<int()> fn) {
        read_char_fn = fn ? std::move(fn) : std::function<int()>(DefaultReadChar);
    }

    void SetWriteCharHandler(std::function<void(unsigned char)> fn) {
        write_char_fn = fn ? std::move(fn) : std::function<void(unsigned char)>(DefaultWriteChar);
    }

    bool StepOnce() {
        if (halted) return false;
        ExecuteOne();
        return !halted;
    }

    void Step() {
        if (!running) return;
        if (halted) {
            running = false;
            return;
        }
        ExecuteOne();
    }

    void Run(int max_steps = -1) {
        if (max_steps < 0) max_steps = Config::MAX_STEPS;
        if (halted) return;

        running = true;
        int steps = 0;
        while (running && !halted && steps < max_steps) {
            ExecuteOne();
            steps++;
        }
        if (running && steps >= max_steps) {
            running = false;
            throw RuntimeError("Max steps exceeded (" + std::to_string(max_steps) +
                               "); possible infinite loop");
        }
    }

    void Simulation() { Run(); }

    void ShowFSMStatus() const {
        for (int i = 0; i < 8; i++) {
            std::cout << "R" << i << ": " << Utilities::MemToHexString(R[i]) << "\n";
        }
        std::cout << "PC: " << Utilities::MemToHexString(PC) << "\n";
        std::cout << "IR: " << Utilities::MemToHexString(IR) << "\n";
        std::cout << "CC: " << CCToString() << "\n";
        std::cout << "HALTED: " << (halted ? "true" : "false") << "\n";
    }

    std::string CCToString() const {
        return CCToString(CC);
    }

    static std::string CCToString(CC_type cc) {
        switch (cc) {
            case NEG: return "n";
            case ZERO: return "z";
            case POS: return "p";
        }
        return "?";
    }

private:
    static int DefaultReadChar() {
        int c = std::cin.get();
        return (c == EOF) ? 0 : (c & 0xFF);
    }

    static void DefaultWriteChar(unsigned char c) {
        std::cout << static_cast<char>(c);
        std::cout.flush();
    }

    static void CheckAddress(int location, const std::string& context) {
        if (location < 0 || location >= MEMORY_SIZE) {
            throw RuntimeError(context + ": address out of range: x" +
                               Utilities::IntToHex(location, 4));
        }
    }

    static void CheckRegister(int num) {
        if (num < 0 || num > 7) {
            throw RuntimeError("Register index out of range: " + std::to_string(num));
        }
    }

    int InvokeReadChar() {
        if (!read_char_fn) return 0;
        return read_char_fn() & 0xFF;
    }

    void InvokeWriteChar(unsigned char c) {
        if (write_char_fn) write_char_fn(c);
    }

    void SetCC(int data) {
        int v = data & 0xFFFF;
        if (v == 0) CC = ZERO;
        else if (v & 0x8000) CC = NEG;
        else CC = POS;
    }

    void SetCC(const MemCell& m) {
        SetCC(Utilities::ReadMem(m));
    }

    void ExecuteOne() {
        int pc_addr = Utilities::ReadMem(PC);
        CheckAddress(pc_addr, "PC");
        Utilities::CopyMem(mem[pc_addr], IR);
        int data = Utilities::ReadMem(IR);
        int opcode = (data >> 12) & 0xF;

        switch (opcode) {
            case 0b0001: ADD(); break;
            case 0b0101: AND(); break;
            case 0b0000: BR(); break;
            case 0b1100: JMP_RET(); break;
            case 0b0100: JSR_JSRR(); break;
            case 0b0010: LD(); break;
            case 0b1010: LDI(); break;
            case 0b0110: LDR(); break;
            case 0b1110: LEA(); break;
            case 0b1001: NOT(); break;
            case 0b1000: RTI(); break;
            case 0b0011: ST(); break;
            case 0b1011: STI(); break;
            case 0b0111: STR(); break;
            case 0b1111: TRAP(); break;
            default:
                throw RuntimeError("Invalid opcode: 0x" + Utilities::IntToHex(opcode, 1));
        }

        if (Config::TRACE) {
            std::cout << "IR=" << Utilities::MemToHexString(IR)
                      << " PC=" << Utilities::MemToHexString(PC) << " ";
            for (int i = 0; i < 8; i++) {
                std::cout << "R" << i << "=" << Utilities::MemToHexString(R[i]) << " ";
            }
            std::cout << "CC=" << CCToString() << "\n";
        }
    }

    void ADD() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int sr1 = Utilities::ReadMem(IR, 8, 3);
        int type = Utilities::ReadMem(IR, 5, 1);
        int result;
        if (type == 0) {
            int sr2 = Utilities::ReadMem(IR, 2, 3);
            result = Utilities::ReadMem(R[sr1]) + Utilities::ReadMem(R[sr2]);
        } else {
            int imm_ext = Utilities::SignExt(Utilities::ReadMem(IR, 4, 5), 5);
            result = Utilities::ReadMem(R[sr1]) + imm_ext;
        }
        Utilities::WriteMem(R[dr], result);
        SetCC(result);
    }

    void AND() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int sr1 = Utilities::ReadMem(IR, 8, 3);
        int type = Utilities::ReadMem(IR, 5, 1);
        int result;
        if (type == 0) {
            int sr2 = Utilities::ReadMem(IR, 2, 3);
            result = Utilities::ReadMem(R[sr1]) & Utilities::ReadMem(R[sr2]);
        } else {
            int imm_ext = Utilities::SignExt(Utilities::ReadMem(IR, 4, 5), 5);
            result = Utilities::ReadMem(R[sr1]) & imm_ext;
        }
        Utilities::WriteMem(R[dr], result);
        SetCC(result);
    }

    void BR() {
        Utilities::IncreaseMem(PC, 1);
        int n = Utilities::ReadMem(IR, 11, 1);
        int z = Utilities::ReadMem(IR, 10, 1);
        int p = Utilities::ReadMem(IR, 9, 1);
        bool match = (n && CC == NEG) || (z && CC == ZERO) || (p && CC == POS);
        if (match) {
            int offset_ext = Utilities::SignExt(Utilities::ReadMem(IR, 8, 9), 9);
            Utilities::IncreaseMem(PC, offset_ext);
        }
    }

    void JMP_RET() {
        Utilities::IncreaseMem(PC, 1);
        int baser = Utilities::ReadMem(IR, 8, 3);
        Utilities::CopyMem(R[baser], PC);
    }

    void JSR_JSRR() {
        Utilities::IncreaseMem(PC, 1);
        int type = Utilities::ReadMem(IR, 11, 1);
        if (type == 0) {
            int baser = Utilities::ReadMem(IR, 8, 3);
            MemCell target = R[baser];
            Utilities::CopyMem(PC, R[7]);
            Utilities::CopyMem(target, PC);
        } else {
            Utilities::CopyMem(PC, R[7]);
            int offset_ext = Utilities::SignExt(Utilities::ReadMem(IR, 10, 11), 11);
            Utilities::IncreaseMem(PC, offset_ext);
        }
    }

    void LD() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int addr = PcRelativeAddress(9);
        Utilities::CopyMem(mem[addr], R[dr]);
        SetCC(R[dr]);
    }

    void LDI() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int ptr_addr = PcRelativeAddress(9);
        int target_addr = Utilities::ReadMem(mem[ptr_addr]);
        Utilities::CopyMem(mem[target_addr], R[dr]);
        SetCC(R[dr]);
    }

    void LDR() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int baser = Utilities::ReadMem(IR, 8, 3);
        int offset_ext = Utilities::SignExt(Utilities::ReadMem(IR, 5, 6), 6);
        int addr = (Utilities::ReadMem(R[baser]) + offset_ext) & 0xFFFF;
        Utilities::CopyMem(mem[addr], R[dr]);
        SetCC(R[dr]);
    }

    void LEA() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        MemCell addr_cell = PC;
        int offset_ext = Utilities::SignExt(Utilities::ReadMem(IR, 8, 9), 9);
        Utilities::IncreaseMem(addr_cell, offset_ext);
        Utilities::CopyMem(addr_cell, R[dr]);
        SetCC(R[dr]);
    }

    void NOT() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int sr = Utilities::ReadMem(IR, 8, 3);
        int result = ~Utilities::ReadMem(R[sr]);
        Utilities::WriteMem(R[dr], result);
        SetCC(result);
    }

    void RTI() {
        running = false;
        throw RuntimeError("RTI is not implemented in this simulator");
    }

    void ST() {
        Utilities::IncreaseMem(PC, 1);
        int sr = Utilities::ReadMem(IR, 11, 3);
        int addr = PcRelativeAddress(9);
        Utilities::CopyMem(R[sr], mem[addr]);
    }

    void STI() {
        Utilities::IncreaseMem(PC, 1);
        int sr = Utilities::ReadMem(IR, 11, 3);
        int ptr_addr = PcRelativeAddress(9);
        int target_addr = Utilities::ReadMem(mem[ptr_addr]);
        Utilities::CopyMem(R[sr], mem[target_addr]);
    }

    void STR() {
        Utilities::IncreaseMem(PC, 1);
        int sr = Utilities::ReadMem(IR, 11, 3);
        int baser = Utilities::ReadMem(IR, 8, 3);
        int offset_ext = Utilities::SignExt(Utilities::ReadMem(IR, 5, 6), 6);
        int addr = (Utilities::ReadMem(R[baser]) + offset_ext) & 0xFFFF;
        Utilities::CopyMem(R[sr], mem[addr]);
    }

    void TRAP() {
        Utilities::IncreaseMem(PC, 1);
        int vec = Utilities::ReadMem(IR, 7, 8);
        Utilities::CopyMem(PC, R[7]);
        switch (vec) {
            case 0x20: TRAP_GETC(); break;
            case 0x21: TRAP_OUT(); break;
            case 0x22: TRAP_PUTS(); break;
            case 0x23: TRAP_IN(); break;
            case 0x24: TRAP_PUTSP(); break;
            case 0x25:
                halted = true;
                running = false;
                break;
            default:
                throw RuntimeError("Unknown TRAP vector: 0x" + Utilities::IntToHex(vec, 2));
        }
    }

    int PcRelativeAddress(int bits) const {
        int start_bit = bits - 1;
        int offset_ext = Utilities::SignExt(Utilities::ReadMem(IR, start_bit, bits), bits);
        return (Utilities::ReadMem(PC) + offset_ext) & 0xFFFF;
    }

    void TRAP_GETC() {
        int c = InvokeReadChar();
        Utilities::WriteMem(R[0], c & 0xFF);
    }

    void TRAP_OUT() {
        int data = Utilities::ReadMem(R[0]);
        InvokeWriteChar(static_cast<unsigned char>(data & 0xFF));
    }

    void TRAP_PUTS() {
        int addr = Utilities::ReadMem(R[0]);
        for (int safety = 0; safety < MEMORY_SIZE; safety++) {
            int word = Utilities::ReadMem(mem[addr & 0xFFFF]);
            if ((word & 0xFFFF) == 0) break;
            InvokeWriteChar(static_cast<unsigned char>(word & 0xFF));
            addr = (addr + 1) & 0xFFFF;
        }
    }

    void TRAP_IN() {
        const char* prompt = "Input a character> ";
        for (const char* p = prompt; *p; p++) InvokeWriteChar(static_cast<unsigned char>(*p));
        int c = InvokeReadChar();
        InvokeWriteChar(static_cast<unsigned char>(c & 0xFF));
        InvokeWriteChar('\n');
        Utilities::WriteMem(R[0], c & 0xFF);
    }

    void TRAP_PUTSP() {
        int addr = Utilities::ReadMem(R[0]);
        for (int safety = 0; safety < MEMORY_SIZE; safety++) {
            int word = Utilities::ReadMem(mem[addr & 0xFFFF]);
            unsigned char low = static_cast<unsigned char>(word & 0xFF);
            unsigned char high = static_cast<unsigned char>((word >> 8) & 0xFF);
            if (low == 0) break;
            InvokeWriteChar(low);
            if (high == 0) break;
            InvokeWriteChar(high);
            addr = (addr + 1) & 0xFFFF;
        }
    }
};

#ifndef LC3_AS_LIBRARY
int main() {
    try {
        std::string start_fill_raw;
        if (!(std::cin >> start_fill_raw)) {
            start_fill_raw = "0011000000000000";
        }
        if (start_fill_raw.size() != 16) {
            throw RuntimeError("First input line must be a 16-bit binary string");
        }
        auto tmp = Utilities::Get16BitFromStr(start_fill_raw);
        int start_fill = (std::to_integer<int>(tmp.first) << 8) |
                         std::to_integer<int>(tmp.second);

        StateMachine s("input.txt", StateMachine::MEMORY_SIZE, start_fill);
        s.Run();

        std::string query;
        while (std::cin >> query) {
            if (query == "ciallo") break;
            try {
                if (query.size() >= 2 && query[0] == 'R') {
                    int r = query[1] - '0';
                    std::cout << Utilities::MemToHexString(s.ReadRegister(r));
                } else if (query == "PC") {
                    std::cout << Utilities::MemToHexString(s.ReadPC());
                } else if (!query.empty() && query[0] == 'x') {
                    int loc = Utilities::HexStringToInt(query);
                    std::cout << Utilities::MemToHexString(s.ReadSingleMem(loc));
                }
            } catch (const RuntimeError& e) {
                std::cout << "<error: " << e.what() << ">";
            }
            std::cout << "\n";
        }
        return 0;
    } catch (const RuntimeError& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 2;
    }
}
#endif
