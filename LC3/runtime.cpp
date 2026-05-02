#include <algorithm>
#include <vector>
#include <set>
#include <string>
#include <cassert>
#include <iostream>
#include <fstream>
#include <iosfwd>
#include <sstream>
#include <iomanip>
// #include <bits/stdc++.h>

using namespace std;

class Config {
public:
    static bool READ_FROM_FILE;
    static string HALT_INSTRUCTION;
    static bool TRACE;
};

class CodeReader {
    static bool isValidLine(const string &line) {
        // check if a line contains a valid lc-3 instruction
        // (str contains only '0' and '1', and length is 16)
        if (line.size() != 16) return false;
        for (const char c: line) {
            if (c != '0' && c != '1') {
                assert(false);
            }
        }
        return true;
    }

public:
    static vector<string> ReadFile(const string &filename) {
        ifstream file(filename);
        if (!file.is_open())
            assert(false);

        vector<string> result;
        string line;

        while (getline(file, line)) {
            if (line.empty()) continue;
            if (!isValidLine(line)) {
                assert(false);
            }
            result.push_back(line);
        }
        return result;
    }

    static vector<string> ReadFromConsole() {
        vector<string> result;
        string line;
        while (cin>>line) {
            if (line=="stop") break;
            result.emplace_back(line);
            if (line==Config::HALT_INSTRUCTION) break;
        }
        return result;
    }
};

class Utilities {
public:
    static pair<byte, byte> Get16BitFromStr(const string &s) {
        // convert a '0'/'1' str to a pair of bytes
        unsigned char b1 = 0;
        unsigned char b2 = 0;
        for (int i = 0; i < 8; ++i) {
            if (s[i] == '1') {
                b1 |= (1 << (7 - i));
            }
        }
        for (int i = 8; i < 16; ++i) {
            if (s[i] == '1') {
                b2 |= (1 << (15 - i));
            }
        }
        return {static_cast<byte>(b1), static_cast<byte>(b2)};
    }

    static void WriteMem(pair<byte, byte> &m, const int data) {
        // write int data into a memory block
        // Arg:data range: 0-65535
        int high = 0b1111111100000000;
        int low = 0b0000000011111111;
        high &= data; // read high 8 bits
        high >>= 8;
        low &= data; // read low 8 bits
        const auto high_byte{static_cast<byte>(high)};
        const auto low_byte{static_cast<byte>(low)};
        m.first = high_byte;
        m.second = low_byte;
    }

    static int ReadMem(const pair<byte, byte> &m) {
        // read memory block as a integer
        int result = 0;
        result = static_cast<int>(m.first); // read high 8 bits
        result <<= 8;
        result |= static_cast<int>(m.second); //read low 8 bits
        return result;
    }

    static void IncreaseMem(pair<byte, byte> &m, const int increase) {
        // add a memory block by a integer (both as complement code)
        int result = 0;
        result = static_cast<int>(m.first);
        result <<= 8;
        result |= static_cast<int>(m.second);
        result += increase; //calculate result
        // int32 contains 32 bits, which is more than lc-3 addressability
        // addition operation of int32 guarantee the correctness of lower 16 bits
        WriteMem(m, result);
    }

    static int ReadMem(const pair<byte, byte> &m, const int start, const int length) {
        // read a memory block from a given bit, and for a given length
        // output as integer
        const int end = start - length;
        int original = ReadMem(m);
        original >>= end + 1;
        int result = 0;
        int rest = length;
        int pow = 0;
        while (rest) {
            if (original & 0x01) {
                result += (1 << pow);
            }
            original >>= 1;
            pow++;
            rest--;
        }
        return result;
    }

    static int SignExt(const int com, const int com_length, const int dst_length = 16) {
        // sign extend a "dst_length"-bit complement code to 16 bit complement code
        const int max_positive = (1 << (com_length - 1)) - 1;
        if (com <= max_positive) return com;
        const int ori = com - (1 << com_length);
        return (1 << dst_length) + ori;
    }

    static void ClearMem(pair<byte, byte> &src) {
        byte zero{0};
        src = {zero, zero};
    }

    static void CopyMem(const pair<byte, byte> &src, pair<byte, byte> &dst) {
        // copy memory block from dst to src
        byte high{src.first};
        byte low{src.second};
        dst = {high, low};
    }

    static string MemToHexString(const pair<std::byte, std::byte> &data) {
        // convert a 2-byte memory block to a string in hex format
        stringstream ss;
        ss << "x";
        ss << std::hex << std::nouppercase << std::setfill('0');
        ss << std::setw(2) << std::to_integer<int>(data.first);
        ss << std::setw(2) << std::to_integer<int>(data.second);
        return ss.str();
    }

    static int HexStringToInt(const string& str) {
        assert(str.size()==5);
        assert(str[0]=='x');
        int n1=(str[1]>='0'&&str[1]<='9')?str[1]-'0':str[1]-'a'+10;
        int n2=(str[2]>='0'&&str[2]<='9')?str[2]-'0':str[2]-'a'+10;
        int n3=(str[3]>='0'&&str[3]<='9')?str[3]-'0':str[3]-'a'+10;
        int n4=(str[4]>='0'&&str[4]<='9')?str[4]-'0':str[4]-'a'+10;
        return n1*4096+n2*256+n3*16+n4;
    }
};

class StateMachine {
    // simulate the state machine of lc-3
    // a great lot of registers are not implemented...
    vector<pair<byte, byte> > mem;
    pair<byte, byte> PC;
    pair<byte, byte> R[8];
    pair<byte, byte> IR;
    bool running;

    enum CC_type {
        POS, ZERO, NEG
    } CC;

public:
    auto ReadSingleMem(int location) const {
        return mem[location];
    }

    auto ReadRegister(int num) const {
        if (num<=0||num>7) num=0;
        return R[num];
    }

    auto ReadPC() const {
        return PC;
    }

    StateMachine(const string &inputPath, const int length, const int  copy_start=0x3000) {
        // initialize the state machine
        vector<string> ins;
        if (Config::READ_FROM_FILE) {
            ins=CodeReader::ReadFile(inputPath);
        }else {
            ins=CodeReader::ReadFromConsole();
        }
        mem.assign(length, {byte{0}, byte{0}}); // initialize all mem to 0
        const int run_start = 0x3000; // by default, program begins at 0x3000

        for (int i = 0; i < ins.size(); i++) {
            // fill the memory modified by the program
            mem[i + copy_start] = Utilities::Get16BitFromStr(ins[i]);
        }
        int valid_sum = 0;
        for (auto [fst, snd]: mem) {
            auto high = static_cast<unsigned char>(fst);
            auto low = static_cast<unsigned char>(snd);
            if (high == 0 && low == 0) {
                continue;
            }
            valid_sum++;
        }
        Utilities::WriteMem(this->PC, run_start);
        for (auto &reg: this->R) {
            // clear all the 8 registers
            Utilities::WriteMem(reg, 0);
        }
        running = false;
        Utilities::WriteMem(IR, 0);
        CC = ZERO;

        if (Config::TRACE) {
            cout << "simulated memory length: " << mem.size() << endl;
            cout << "filled memory block sum: " << valid_sum << endl;
        }
    }

private:
    void SetCC(const int data) {
        // set CC with a integer data
        if (data == 0) {
            this->CC = ZERO;
        } else if (data & 0x80) {
            this->CC = NEG;
        } else {
            this->CC = POS;
        }
    }

    void SetCC(const pair<byte, byte> &data) {
        // set CC with a memory block with 2 bytes
        if (static_cast<int>(data.first) == 0 && static_cast<int>(data.second) == 0) {
            this->CC = ZERO;
        } else {
            if (static_cast<int>(data.first >> 7)) {
                this->CC = NEG;
            } else {
                this->CC = POS;
            }
        }
    }

    void ADD() {
        Utilities::IncreaseMem(PC, 1);
        int r1 = Utilities::ReadMem(IR, 11, 3);
        int r2 = Utilities::ReadMem(IR, 8, 3);
        int type = Utilities::ReadMem(IR, 5, 1);
        if (type == 0) {
            // register mode
            int r3 = Utilities::ReadMem(IR, 2, 3);
            int result = Utilities::ReadMem(R[r2]) + Utilities::ReadMem(R[r3]);
            Utilities::WriteMem(R[r1], result);
            SetCC(result);
        } else if (type == 1) {
            // immediate number mode
            int imme_com = Utilities::ReadMem(IR, 4, 5);
            int imme_ext = Utilities::SignExt(imme_com, 5);
            auto result = R[r2];
            Utilities::IncreaseMem(result, imme_ext);
            Utilities::CopyMem(result, R[r1]);
            SetCC(result);
        }
    }

    void AND() {
        Utilities::IncreaseMem(PC, 1);
        int r1 = Utilities::ReadMem(IR, 11, 3);
        int r2 = Utilities::ReadMem(IR, 8, 3);
        int type = Utilities::ReadMem(IR, 5, 1);
        if (type == 0) {
            // register mode
            int r3 = Utilities::ReadMem(IR, 2, 3);
            int result = Utilities::ReadMem(R[r2]) & Utilities::ReadMem(R[r3]);
            Utilities::WriteMem(R[r1], result);
            SetCC(result);
        } else if (type == 1) {
            // immediate number mode
            int imme_com = Utilities::ReadMem(IR, 4, 5);
            int imme_ext = Utilities::SignExt(imme_com, 5);
            auto result = Utilities::ReadMem(R[r2]);
            result &= imme_ext;
            Utilities::WriteMem(R[r1], result);
            SetCC(result);
        }
    }

    void BR() {
        Utilities::IncreaseMem(PC, 1);
        int n = Utilities::ReadMem(IR, 11, 1);
        int z = Utilities::ReadMem(IR, 10, 1);
        int p = Utilities::ReadMem(IR, 9, 1);
        bool match = (n && CC == NEG) || (z && CC == ZERO) || (p && CC == POS);
        if (match) {
            const int offset_com = Utilities::ReadMem(IR, 8, 9);
            const int offset_ext = Utilities::SignExt(offset_com, 9);
            Utilities::IncreaseMem(PC, offset_ext);
        }
    }

    void JMP_RET() {
        //JMP and RET share the same format of instruction
        Utilities::IncreaseMem(PC, 1);
        int r = Utilities::ReadMem(IR, 8, 3);
        Utilities::CopyMem(R[r], PC);
    }

    void JSR_JSRR() {
        if (Config::TRACE) {
            cout << "IR " << hex << Utilities::ReadMem(IR) << endl;
        }
        Utilities::IncreaseMem(PC, 1);
        Utilities::CopyMem(PC, R[7]);
        int type = Utilities::ReadMem(IR, 11, 1);
        if (type == 0) {
            //JSRR
            int r = Utilities::ReadMem(IR, 8, 3);
            Utilities::CopyMem(R[r], PC);
        } else if (type == 1) {
            //JSR
            int offset_com = Utilities::ReadMem(IR, 10, 11);
            int offset_ext = Utilities::SignExt(offset_com, 11);
            Utilities::IncreaseMem(PC, offset_ext);
        }
    }

    void LD() {
        Utilities::IncreaseMem(PC, 1);
        int r = Utilities::ReadMem(IR, 11, 3);
        int offset_com = Utilities::ReadMem(IR, 8, 9);
        int offset_ext = Utilities::SignExt(offset_com, 9);
        auto addr_byte = PC;
        Utilities::IncreaseMem(addr_byte, offset_ext);
        int addr = Utilities::ReadMem(addr_byte);
        Utilities::CopyMem(mem[addr], R[r]);
        SetCC(mem[addr]);
    }

    void LDI() {
        Utilities::IncreaseMem(PC, 1);
        int r = Utilities::ReadMem(IR, 11, 3);
        int offset_com = Utilities::ReadMem(IR, 8, 9);
        int offset_ext = Utilities::SignExt(offset_com, 9);
        auto addr_addr_byte = PC;
        Utilities::IncreaseMem(addr_addr_byte, offset_ext);
        int addr_addr = Utilities::ReadMem(addr_addr_byte);
        auto addr_byte = mem[addr_addr];
        int addr = Utilities::ReadMem(addr_byte);
        Utilities::CopyMem(mem[addr], R[r]);
        SetCC(mem[addr]);
    }

    void LDR() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int baser = Utilities::ReadMem(IR, 8, 3);
        int offset_com = Utilities::ReadMem(IR, 5, 6);
        int offset_ext = Utilities::SignExt(offset_com, 6);
        auto addr_byte = R[baser];
        Utilities::IncreaseMem(addr_byte, offset_ext);
        int addr = Utilities::ReadMem(addr_byte);
        Utilities::CopyMem(mem[addr], R[dr]);
        SetCC(mem[addr]);
    }

    void LEA() {
        Utilities::IncreaseMem(PC, 1);
        int r = Utilities::ReadMem(IR, 11, 3);
        int offset_com = Utilities::ReadMem(IR, 8, 9);
        int offset_ext = Utilities::SignExt(offset_com, 9);
        auto addr_byte = PC;
        Utilities::IncreaseMem(addr_byte, offset_ext);
        Utilities::CopyMem(addr_byte, R[r]);
    }

    void NOT() {
        Utilities::IncreaseMem(PC, 1);
        int dr = Utilities::ReadMem(IR, 11, 3);
        int sr = Utilities::ReadMem(IR, 8, 3);
        int data = Utilities::ReadMem(R[sr]);
        int result = ~data;
        Utilities::WriteMem(R[dr], result);
        SetCC(data);
    }

    void RTI() {
        //no need to implement now...
    }

    void ST() {
        Utilities::IncreaseMem(PC, 1);
        int r = Utilities::ReadMem(IR, 11, 3);
        int offset_com = Utilities::ReadMem(IR, 8, 9);
        int offset_ext = Utilities::SignExt(offset_com, 9);
        auto addr_byte = PC;
        Utilities::IncreaseMem(addr_byte, offset_ext);
        int addr = Utilities::ReadMem(addr_byte);
        Utilities::CopyMem(R[r], mem[addr]);
    }

    void STI() {
        Utilities::IncreaseMem(PC, 1);
        int r = Utilities::ReadMem(IR, 11, 3);
        int offset_com = Utilities::ReadMem(IR, 8, 9);
        int offset_ext = Utilities::SignExt(offset_com, 9);
        auto addr_addr_byte = PC;
        Utilities::IncreaseMem(addr_addr_byte, offset_ext);
        int addr_addr = Utilities::ReadMem(addr_addr_byte);
        auto addr_byte = mem[addr_addr];
        int addr = Utilities::ReadMem(addr_byte);
        Utilities::CopyMem(R[r], mem[addr]);
    }

    void STR() {
        Utilities::IncreaseMem(PC, 1);
        int sr = Utilities::ReadMem(IR, 11, 3);
        int baser = Utilities::ReadMem(IR, 8, 3);
        int offset_com = Utilities::ReadMem(IR, 5, 6);
        int offset_ext = Utilities::SignExt(offset_com, 6);
        auto addr_byte = R[baser];
        Utilities::IncreaseMem(addr_byte, offset_ext);
        int addr = Utilities::ReadMem(addr_byte);
        Utilities::CopyMem(R[sr], mem[addr]);
    }

    void TRAP() {
        Utilities::IncreaseMem(PC, 1);
        int vec = Utilities::ReadMem(IR, 7, 8);
        switch (vec) {
            case 32: cout << "getc" << endl; // not implemented yet...
                break;
            case 33: cout << "out" << endl; // not implemented yet...
                break;
            case 34: cout << "puts" << endl; // not implemented yet...
                break;
            case 35: cout << "in" << endl; // not implemented yet...
                break;
            case 36: cout << "putsp" << endl; // not implemented yet...
                break;
            case 37: running = false;
                break;
            default: assert(false);
        }
    }

public:
    void WriteSingleMem(const int pos, const int data) {
        assert(pos>=0&&pos<=0xFFFF);
        Utilities::WriteMem(mem[pos], data);
    }

    void Simulation() {
        // run the lc-3!
        int round = 0; // for debug
        running = true;
        while (running) {
            if (Config::TRACE) {
                cout << "round " << round++ << " ";
            }
            const auto start_addr_byte = Utilities::ReadMem(PC);
            Utilities::CopyMem(mem[start_addr_byte], IR); //copy mem[PC] to IR
            int data = Utilities::ReadMem(IR);
            switch (data >> 12) {
                // just like a MUX...
                case 0b0001: ADD();
                    break;
                case 0b0101: AND();
                    break;
                case 0b0000: BR();
                    break;
                case 0b1100: JMP_RET();
                    break;
                case 0b0100: JSR_JSRR();
                    break;
                case 0b0010: LD();
                    break;
                case 0b1010: LDI();
                    break;
                case 0b0110: LDR();
                    break;
                case 0b1110: LEA();
                    break;
                case 0b1001: NOT();
                    break;
                case 0b1000: RTI();
                    break;
                case 0b0011: ST();
                    break;
                case 0b1011: STI();
                    break;
                case 0b0111: STR();
                    break;
                case 0b1111: TRAP();
                    break;
                default: assert(false);
            }
            // for debug:
            if (Config::TRACE) {
                cout << "ins: ";
                cout << Utilities::MemToHexString(IR);
                cout << " PC: ";
                cout << Utilities::MemToHexString(PC) << "  ";
                for (int i = 0; i < 8; i++) {
                    auto d = R[i];
                    cout << Utilities::MemToHexString(R[i]) << " ";
                }
                cout << endl;
            }
        }
    }

    void ShowFSMStatus() const {
        // show all the registers info simulated
        for (int i = 0; i < 8; i++) {
            cout << "R" << i << ": " << Utilities::MemToHexString(R[i]) << endl;
        }
        cout << "PC: " << Utilities::MemToHexString(PC) << endl;
        cout << "CC: ";
        if (CC == ZERO) {
            cout << "z";
        } else if (CC == POS) {
            cout << "p";
        } else {
            cout << "n";
        }
        cout << endl;
    }
};

bool Config::READ_FROM_FILE=false;
string Config::HALT_INSTRUCTION="1111000000100101";
bool Config::TRACE=false;

int test_for_lab4() {
    string start_fill_raw;
    cin>>start_fill_raw;
    auto tmp= Utilities::Get16BitFromStr(start_fill_raw);
    const int start_fill = (static_cast<int>(tmp.first) << 8) + static_cast<int>(tmp.second);
    string inputPath = "input.txt";
    StateMachine s(inputPath, 0xFFFF, start_fill);
    // write certain memory as input (ics lab 4)
    // s.WriteSingleMem(0x3100, 0x0005);
    // s.WriteSingleMem(0x3101, 0x0005);
    s.Simulation();
    s.ShowFSMStatus();
    // show output in mem[x3200] (ics lab 4)
    // for (int i = 0x3200; i <= 0x3200; i++) {
    //     cout << "mem[" << hex << i << "] ";
    //     cout << Utilities::MemToHexString(s.ReadSingleMem(i)) << endl;
    // }
    return 0;
}

int main() {
    string start_fill_raw;
    cin>>start_fill_raw;
    auto tmp= Utilities::Get16BitFromStr(start_fill_raw);
    const int start_fill = (static_cast<int>(tmp.first) << 8) + static_cast<int>(tmp.second);
    string inputPath = "input.txt";
    StateMachine s(inputPath, 0xFFFF, start_fill);
    s.Simulation();
    // s.ShowFSMStatus();
    string query;
    while (cin>>query) {
        if (query=="ciallo") break;
        if (query.size()<2) break;
        if (query[0]=='R') {
            int r=query[1]-'0';
            cout<<Utilities::MemToHexString(s.ReadRegister(r));
        }else if (query=="PC") {
            cout<<Utilities::MemToHexString(s.ReadPC());
        }else if (query[0]=='x') {
            int mem_location=Utilities::HexStringToInt(query);
            cout<<Utilities::MemToHexString(s.ReadSingleMem(mem_location));
        }
        cout<<"\n";
    }
    return 0;
}
