// LC-3 assembler.
//
// GUI-facing API:
//   Parser p; // tries "LC3/config.json", then "config.json"
//   std::string code = p.AssembleSource(source);
//
// AssembleSource returns one 16-bit binary word per line. Errors are reported
// with AssemblyError exceptions.

#include <bitset>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

class AssemblyError : public std::runtime_error {
public:
    explicit AssemblyError(const std::string& msg) : std::runtime_error(msg) {}
};

class Configuration {
public:
    static bool OutputBitCnt;
    static bool TraceWord;
    static bool TraceLabel;
    static bool OutputHex;
    static bool OutputX3000;
    static bool InputFromConsole;
    static bool OutputToConsole;
    static bool CheckImmediateRange;
};

bool Configuration::OutputBitCnt = false;
bool Configuration::TraceLabel = false;
bool Configuration::TraceWord = false;
bool Configuration::OutputHex = false;
bool Configuration::OutputX3000 = true;
bool Configuration::InputFromConsole = false;
bool Configuration::OutputToConsole = false;
bool Configuration::CheckImmediateRange = true;

class Tokenizer {
public:
    static std::vector<std::string> SplitLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        return lines;
    }

    static std::vector<std::string> TokenizeLine(const std::string& line) {
        std::vector<std::string> tokens;
        std::string current_token;
        bool in_string = false;
        bool escape = false;

        for (char c : line) {
            if (in_string) {
                if (escape) {
                    switch (c) {
                        case '"': current_token += '"'; break;
                        case '\\': current_token += '\\'; break;
                        case 'n': current_token += '\n'; break;
                        case 't': current_token += '\t'; break;
                        case 'r': current_token += '\r'; break;
                        case '0': current_token += '\0'; break;
                        case 'a': current_token += '\a'; break;
                        case 'b': current_token += '\b'; break;
                        case 'f': current_token += '\f'; break;
                        case 'v': current_token += '\v'; break;
                        default:
                            current_token += '\\';
                            current_token += c;
                            break;
                    }
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    current_token += c;
                    tokens.push_back(current_token);
                    current_token.clear();
                    in_string = false;
                } else {
                    current_token += c;
                }
                continue;
            }

            if (c == ';') break;
            if (c == '"') {
                if (!current_token.empty()) {
                    tokens.push_back(current_token);
                    current_token.clear();
                }
                current_token += c;
                in_string = true;
            } else if (c == ' ' || c == ',' || c == '\t') {
                if (!current_token.empty()) {
                    tokens.push_back(current_token);
                    current_token.clear();
                }
            } else {
                current_token += c;
            }
        }

        if (in_string) {
            throw AssemblyError("Unterminated string literal");
        }
        if (!current_token.empty()) {
            tokens.push_back(current_token);
        }
        return tokens;
    }

    static std::string ReadFileIntoString(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            throw AssemblyError("Cannot open file: " + filename);
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    static std::string LowercaseToken(const std::string& token) {
        if (token.length() >= 2 && token.front() == '"' && token.back() == '"') {
            return token;
        }
        std::string result = token;
        for (char& c : result) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return result;
    }
};

class Parser {
    struct SourceLine {
        int source_line = 0;
        std::vector<std::string> tokens;
    };

    struct ParsedLine {
        enum Kind { Empty, End, Statement } kind = Empty;
        int word_count = 0;
        std::vector<std::string> encoded;
    };

    std::map<std::string, std::string> valid_token;
    std::map<std::string, int> label_line;

    static constexpr int DEFAULT_ORIGIN = 0x3000;

public:
    Parser() {
        try {
            LoadConfig("LC3/config.json");
        } catch (const AssemblyError&) {
            LoadConfig("config.json");
        }
    }

    explicit Parser(const std::string& config_path) {
        LoadConfig(config_path);
    }

    void LoadConfig(const std::string& path) {
        valid_token.clear();
        std::ifstream in(path);
        if (!in) {
            throw AssemblyError("Cannot open config file: " + path);
        }

        json j;
        try {
            in >> j;
        } catch (const std::exception& e) {
            throw AssemblyError(std::string("Invalid JSON in ") + path + ": " + e.what());
        }

        for (auto& [k, v] : j.items()) {
            if (!v.is_string()) {
                throw AssemblyError("Config value for '" + k + "' is not a string");
            }
            valid_token.emplace(k, v.get<std::string>());
        }
    }

    static bool isReg(const std::string& s) {
        return s.length() == 2 && s[0] == 'r' && s[1] >= '0' && s[1] <= '7';
    }

    bool isPseudoInst(const std::string& s) const {
        return valid_token.find(s) != valid_token.end() && s.length() > 1 && s[0] == '.';
    }

    bool isSyntax(const std::string& s) const {
        return valid_token.find(s) != valid_token.end() && !isReg(s) && !isPseudoInst(s);
    }

    static bool isString(const std::string& s) {
        return s.length() >= 2 && s.front() == '"' && s.back() == '"';
    }

    static bool isValidImmediateNum(const std::string& s) {
        if (s.empty()) return false;

        enum NumBase { HEX, DEC, BIN, OCT } type = DEC;
        size_t begin = 0;
        if (s[0] == '#' || s[0] == 'x' || s[0] == 'b' || s[0] == 'o') {
            switch (s[0]) {
                case '#': type = DEC; break;
                case 'x': type = HEX; break;
                case 'b': type = BIN; break;
                case 'o': type = OCT; break;
            }
            if (s.length() <= 1) return false;
            begin = 1;
            if (s[begin] == '-') {
                begin++;
                if (begin >= s.length()) return false;
            }
        } else if (s[0] == '-') {
            if (s.length() <= 1) return false;
            begin = 1;
        } else if (!std::isdigit(static_cast<unsigned char>(s[0]))) {
            return false;
        }

        for (size_t i = begin; i < s.length(); i++) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            switch (type) {
                case OCT: if (c < '0' || c > '7') return false; break;
                case BIN: if (c != '0' && c != '1') return false; break;
                case HEX:
                    if (!((c >= '0' && c <= '9') ||
                          (c >= 'a' && c <= 'f') ||
                          (c >= 'A' && c <= 'F'))) {
                        return false;
                    }
                    break;
                case DEC: if (c < '0' || c > '9') return false; break;
            }
        }
        return true;
    }

    bool isTrap(const std::string& s) const {
        return s == "getc" || s == "out" || s == "puts" ||
               s == "in" || s == "putsp" || s == "halt";
    }

    bool isLabel(const std::string& s) const {
        if (s.empty()) return false;
        if (!(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) return false;
        for (char c : s) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
        }
        return !isReg(s) && !isSyntax(s) && !isPseudoInst(s) && !isTrap(s);
    }

    static int ParseImmediateValue(const std::string& s) {
        if (!isValidImmediateNum(s)) {
            throw AssemblyError("Invalid immediate number: " + s);
        }

        int base = 10;
        std::string num_str;
        switch (s[0]) {
            case '#': base = 10; num_str = s.substr(1); break;
            case 'x': base = 16; num_str = s.substr(1); break;
            case 'o': base = 8; num_str = s.substr(1); break;
            case 'b': base = 2; num_str = s.substr(1); break;
            default: num_str = s; break;
        }

        try {
            long long v = std::stoll(num_str, nullptr, base);
            if (v > INT32_MAX || v < INT32_MIN) {
                throw AssemblyError("Immediate number out of range: " + s);
            }
            return static_cast<int>(v);
        } catch (const AssemblyError&) {
            throw;
        } catch (const std::exception&) {
            throw AssemblyError("Invalid immediate number: " + s);
        }
    }

    static std::string IntToBinStr(int value, int digit) {
        if (digit <= 0 || digit >= 64) {
            throw AssemblyError("Invalid digit count: " + std::to_string(digit));
        }
        unsigned long long uvalue;
        if (value < 0) {
            uvalue = (1ULL << digit) + static_cast<unsigned long long>(static_cast<long long>(value));
        } else {
            uvalue = static_cast<unsigned long long>(value);
        }
        uvalue &= (1ULL << digit) - 1;
        std::bitset<64> bits(uvalue);
        return bits.to_string().substr(64 - digit);
    }

    static void CheckSignedRange(int value, int digit, const std::string& context) {
        if (!Configuration::CheckImmediateRange) return;
        long long min_val = -(1LL << (digit - 1));
        long long max_val = (1LL << (digit - 1)) - 1;
        if (value < min_val || value > max_val) {
            throw AssemblyError(context + ": value " + std::to_string(value) +
                                " out of signed " + std::to_string(digit) +
                                "-bit range [" + std::to_string(min_val) +
                                ", " + std::to_string(max_val) + "]");
        }
    }

    static void CheckWordRange(int value, const std::string& context) {
        if (!Configuration::CheckImmediateRange) return;
        if (value < -32768 || value > 65535) {
            throw AssemblyError(context + ": value " + std::to_string(value) +
                                " out of 16-bit range [-32768, 65535]");
        }
    }

    static std::string GetImmediateNumStr(const std::string& s, int digit) {
        int value = ParseImmediateValue(s);
        CheckSignedRange(value, digit, "Immediate '" + s + "'");
        return IntToBinStr(value, digit);
    }

    static std::string GetImmediateNumStr(int value, int digit) {
        CheckSignedRange(value, digit, "Value " + std::to_string(value));
        return IntToBinStr(value, digit);
    }

    static int GetImmediateNum(const std::string& s) {
        return ParseImmediateValue(s);
    }

    static std::string GetRealString(const std::string& str) {
        if (!isString(str)) {
            throw AssemblyError("Not a quoted string: " + str);
        }
        return str.substr(1, str.length() - 2);
    }

    std::string AssembleSource(const std::string& source) {
        return JoinWords(AssembleSourceToWords(source));
    }

    std::vector<std::string> AssembleSourceToWords(const std::string& source) {
        return AssembleLines(ParseSourceLines(source));
    }

    std::string AssembleSourceFlat(const std::string& source) {
        std::string result;
        for (const std::string& word : AssembleSourceToWords(source)) {
            result += word;
        }
        return result;
    }

    std::string AssembleFile(const std::string& path) {
        std::string content;
        if (Configuration::InputFromConsole) {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line == ".end" || line == ".END") break;
                content += line;
                content += "\n";
            }
        } else {
            content = Tokenizer::ReadFileIntoString(path);
        }
        return AssembleSource(content);
    }

    std::string ParserFromFile(std::string& path) {
        return AssembleFile(path);
    }

private:
    static std::string WithLine(int source_line, const std::string& msg) {
        return "Line " + std::to_string(source_line) + ": " + msg;
    }

    std::vector<SourceLine> ParseSourceLines(const std::string& source) const {
        std::vector<SourceLine> result;
        auto raw_lines = Tokenizer::SplitLines(source);
        for (size_t i = 0; i < raw_lines.size(); i++) {
            SourceLine line;
            line.source_line = static_cast<int>(i + 1);
            try {
                auto tokens = Tokenizer::TokenizeLine(raw_lines[i]);
                for (const std::string& token : tokens) {
                    line.tokens.push_back(Tokenizer::LowercaseToken(token));
                }
            } catch (const AssemblyError& e) {
                throw AssemblyError(WithLine(line.source_line, e.what()));
            }
            result.push_back(std::move(line));
        }
        return result;
    }

    static std::string JoinWords(const std::vector<std::string>& words) {
        std::string result;
        for (const std::string& word : words) {
            result += word;
            result += '\n';
        }
        return result;
    }

    static void RequireCount(const SourceLine& line, size_t got, size_t want, const std::string& mnemonic) {
        if (got != want) {
            throw AssemblyError(WithLine(line.source_line, mnemonic + " requires " +
                                std::to_string(want) + " operand(s), got " +
                                std::to_string(got)));
        }
    }

    std::vector<std::string> AssembleLines(const std::vector<SourceLine>& lines) {
        label_line.clear();
        int current_line = 0;
        bool ended = false;

        for (const SourceLine& line : lines) {
            ParsedLine parsed = ParseStatement(line, current_line, true);
            if (parsed.kind == ParsedLine::End) {
                ended = true;
                break;
            }
            current_line += parsed.word_count;
        }

        (void)ended;

        std::vector<std::string> words;
        current_line = 0;
        for (const SourceLine& line : lines) {
            ParsedLine parsed = ParseStatement(line, current_line, false);
            if (parsed.kind == ParsedLine::End) break;
            if (!parsed.encoded.empty()) {
                words.insert(words.end(), parsed.encoded.begin(), parsed.encoded.end());
            }
            current_line += parsed.word_count;
        }
        return words;
    }

    ParsedLine ParseStatement(const SourceLine& line, int current_line, bool pass1) {
        ParsedLine parsed;
        if (line.tokens.empty()) return parsed;

        size_t pos = 0;
        const std::string& first = line.tokens[pos];
        if (isLabel(first)) {
            if (pass1) {
                if (label_line.find(first) != label_line.end()) {
                    throw AssemblyError(WithLine(line.source_line, "Duplicate label: " + first));
                }
                label_line.emplace(first, current_line);
                if (Configuration::TraceLabel) {
                    std::cout << "label " << first << " at x"
                              << std::hex << (DEFAULT_ORIGIN + current_line) << std::dec << "\n";
                }
            }
            pos++;
            if (pos >= line.tokens.size()) return parsed;
        }

        const std::string& op = line.tokens[pos++];
        std::vector<std::string> args(line.tokens.begin() + static_cast<std::ptrdiff_t>(pos),
                                      line.tokens.end());
        parsed.kind = ParsedLine::Statement;

        if (op == ".end") {
            RequireCount(line, args.size(), 0, ".end");
            parsed.kind = ParsedLine::End;
            return parsed;
        }

        if (op == ".orig") {
            RequireCount(line, args.size(), 1, ".orig");
            int origin = ParseImmediateValue(args[0]);
            if (origin != DEFAULT_ORIGIN) {
                throw AssemblyError(WithLine(line.source_line,
                                    ".orig address must be x3000; custom origins are not supported"));
            }
            return parsed;
        }

        if (op == ".blkw") {
            RequireCount(line, args.size(), 1, ".blkw");
            int n = ParseImmediateValue(args[0]);
            if (n < 0) {
                throw AssemblyError(WithLine(line.source_line, ".blkw count must be non-negative"));
            }
            parsed.word_count = n;
            if (!pass1) {
                parsed.encoded.assign(static_cast<size_t>(n), "0000000000000000");
            }
            return parsed;
        }

        if (op == ".stringz") {
            RequireCount(line, args.size(), 1, ".stringz");
            if (!isString(args[0])) {
                throw AssemblyError(WithLine(line.source_line, ".stringz requires a string literal"));
            }
            std::string content = GetRealString(args[0]);
            parsed.word_count = static_cast<int>(content.length()) + 1;
            if (!pass1) {
                for (unsigned char c : content) {
                    parsed.encoded.push_back(IntToBinStr(c, 16));
                }
                parsed.encoded.push_back("0000000000000000");
            }
            return parsed;
        }

        if (op == ".fill") {
            RequireCount(line, args.size(), 1, ".fill");
            parsed.word_count = 1;
            if (!isValidImmediateNum(args[0]) && !isLabel(args[0])) {
                throw AssemblyError(WithLine(line.source_line, ".fill requires immediate or label"));
            }
            if (!pass1) {
                if (isValidImmediateNum(args[0])) {
                    int value = ParseImmediateValue(args[0]);
                    CheckWordRange(value, ".fill value");
                    parsed.encoded.push_back(IntToBinStr(value, 16));
                } else {
                    auto it = label_line.find(args[0]);
                    if (it == label_line.end()) {
                        throw AssemblyError(WithLine(line.source_line, "Unknown label in .fill: " + args[0]));
                    }
                    parsed.encoded.push_back(IntToBinStr(DEFAULT_ORIGIN + it->second, 16));
                }
            }
            return parsed;
        }

        if (isTrap(op)) {
            RequireCount(line, args.size(), 0, op);
            parsed.word_count = 1;
            if (!pass1) parsed.encoded.push_back(valid_token.at(op));
            return parsed;
        }

        parsed.word_count = 1;
        if (op == "add" || op == "and") {
            RequireCount(line, args.size(), 3, op);
            RequireReg(line, args[0], op + " operand 1");
            RequireReg(line, args[1], op + " operand 2");
            if (!isReg(args[2]) && !isValidImmediateNum(args[2])) {
                throw AssemblyError(WithLine(line.source_line, op + " operand 3 must be register or immediate"));
            }
            if (isValidImmediateNum(args[2])) {
                CheckSignedRange(ParseImmediateValue(args[2]), 5, op + " immediate");
            }
            if (!pass1) {
                std::string word = valid_token.at(op) + valid_token.at(args[0]) + valid_token.at(args[1]);
                if (isReg(args[2])) {
                    word += "000" + valid_token.at(args[2]);
                } else {
                    word += "1" + GetImmediateNumStr(args[2], 5);
                }
                parsed.encoded.push_back(word);
            }
            return parsed;
        }

        if (IsBranch(op)) {
            RequireCount(line, args.size(), 1, op);
            RequireLabelOperand(line, args[0], op + " target");
            if (!pass1) {
                std::string word = valid_token.at(op);
                if (op == "br") {
                    word += "111";
                } else {
                    word += (op.find('n') != std::string::npos) ? "1" : "0";
                    word += (op.find('z') != std::string::npos) ? "1" : "0";
                    word += (op.find('p') != std::string::npos) ? "1" : "0";
                }
                word += EncodePCOffset(line, args[0], current_line, 9, op + " PCoffset");
                parsed.encoded.push_back(word);
            }
            return parsed;
        }

        if (op == "jmp" || op == "jsrr") {
            RequireCount(line, args.size(), 1, op);
            RequireReg(line, args[0], op + " operand");
            if (!pass1) {
                parsed.encoded.push_back(valid_token.at(op) + "000" + valid_token.at(args[0]) + "000000");
            }
            return parsed;
        }

        if (op == "jsr") {
            RequireCount(line, args.size(), 1, op);
            RequireLabelOperand(line, args[0], "jsr target");
            if (!pass1) {
                parsed.encoded.push_back(valid_token.at(op) + "1" +
                                         EncodePCOffset(line, args[0], current_line, 11, "jsr PCoffset"));
            }
            return parsed;
        }

        if (op == "ld" || op == "st" || op == "ldi" || op == "sti" || op == "lea") {
            RequireCount(line, args.size(), 2, op);
            RequireReg(line, args[0], op + " operand 1");
            RequireLabelOperand(line, args[1], op + " target");
            if (!pass1) {
                parsed.encoded.push_back(valid_token.at(op) + valid_token.at(args[0]) +
                                         EncodePCOffset(line, args[1], current_line, 9, op + " PCoffset"));
            }
            return parsed;
        }

        if (op == "ldr" || op == "str") {
            RequireCount(line, args.size(), 3, op);
            RequireReg(line, args[0], op + " operand 1");
            RequireReg(line, args[1], op + " operand 2");
            if (!isValidImmediateNum(args[2])) {
                throw AssemblyError(WithLine(line.source_line, op + " offset must be immediate"));
            }
            int offset = ParseImmediateValue(args[2]);
            CheckSignedRange(offset, 6, op + " offset");
            if (!pass1) {
                parsed.encoded.push_back(valid_token.at(op) + valid_token.at(args[0]) +
                                         valid_token.at(args[1]) + IntToBinStr(offset, 6));
            }
            return parsed;
        }

        if (op == "not") {
            RequireCount(line, args.size(), 2, "not");
            RequireReg(line, args[0], "not operand 1");
            RequireReg(line, args[1], "not operand 2");
            if (!pass1) {
                parsed.encoded.push_back(valid_token.at(op) + valid_token.at(args[0]) +
                                         valid_token.at(args[1]) + "111111");
            }
            return parsed;
        }

        if (op == "ret") {
            RequireCount(line, args.size(), 0, "ret");
            if (!pass1) parsed.encoded.push_back(valid_token.at(op) + "000111000000");
            return parsed;
        }

        if (op == "rti") {
            RequireCount(line, args.size(), 0, "rti");
            if (!pass1) parsed.encoded.push_back(valid_token.at(op) + "000000000000");
            return parsed;
        }

        if (op == "trap") {
            RequireCount(line, args.size(), 1, "trap");
            if (!isValidImmediateNum(args[0])) {
                throw AssemblyError(WithLine(line.source_line, "trap vector must be immediate"));
            }
            int vector = ParseImmediateValue(args[0]);
            if (vector < 0 || vector > 255) {
                throw AssemblyError(WithLine(line.source_line, "trap vector out of range [0, 255]"));
            }
            if (!pass1) parsed.encoded.push_back(valid_token.at(op) + "0000" + IntToBinStr(vector, 8));
            return parsed;
        }

        if (valid_token.find(op) != valid_token.end()) {
            throw AssemblyError(WithLine(line.source_line, "Unsupported instruction: " + op));
        }
        throw AssemblyError(WithLine(line.source_line, "Unknown token: " + op));
    }

    void RequireReg(const SourceLine& line, const std::string& token, const std::string& context) const {
        if (!isReg(token)) {
            throw AssemblyError(WithLine(line.source_line, context + " must be a register, got: " + token));
        }
    }

    void RequireLabelOperand(const SourceLine& line, const std::string& token, const std::string& context) const {
        if (!isLabel(token)) {
            throw AssemblyError(WithLine(line.source_line, context + " must be a label, got: " + token));
        }
    }

    bool IsBranch(const std::string& op) const {
        return op == "br" || op == "brn" || op == "brz" || op == "brp" ||
               op == "brnz" || op == "brnp" || op == "brzp" || op == "brnzp";
    }

    std::string EncodePCOffset(const SourceLine& line, const std::string& label,
                               int current_line, int digit, const std::string& context) const {
        auto it = label_line.find(label);
        if (it == label_line.end()) {
            throw AssemblyError(WithLine(line.source_line, "Unknown label: " + label));
        }
        int offset = it->second - current_line - 1;
        CheckSignedRange(offset, digit, context);
        return IntToBinStr(offset, digit);
    }
};

class Utilities {
public:
    static std::string BinStr2HexStr(const std::string& str) {
        if (str.length() != 16) {
            throw AssemblyError("BinStr2HexStr: input must be 16 chars");
        }
        std::string result;
        for (int i = 0; i < 4; i++) {
            int v = 0;
            for (int b = 0; b < 4; b++) {
                char c = str[i * 4 + b];
                if (c != '0' && c != '1') {
                    throw AssemblyError("BinStr2HexStr: non-binary character in input");
                }
                v = (v << 1) | (c - '0');
            }
            result += (v < 10) ? static_cast<char>('0' + v) : static_cast<char>('A' + v - 10);
        }
        return result;
    }
};

#ifndef LC3_AS_LIBRARY
int main() {
    try {
        Parser p;
        std::string input_path = "input.txt";
        std::string result = p.AssembleFile(input_path);

        std::ostream* out = &std::cout;
        std::ofstream outFile;
        if (!Configuration::OutputToConsole) {
            outFile.open("output.txt");
            if (!outFile) throw AssemblyError("Cannot open output.txt");
            out = &outFile;
        }

        if (Configuration::OutputX3000) {
            *out << "0011000000000000\n";
        }
        if (Configuration::OutputBitCnt) {
            size_t word_count = 0;
            std::istringstream count_stream(result);
            std::string count_word;
            while (std::getline(count_stream, count_word)) {
                if (!count_word.empty()) word_count++;
            }
            *out << (word_count * 16) << "\n";
        }

        std::istringstream iss(result);
        std::string word;
        while (std::getline(iss, word)) {
            if (word.empty()) continue;
            *out << word;
            if (Configuration::OutputHex) {
                *out << "  ;x" << Utilities::BinStr2HexStr(word);
            }
            *out << "\n";
        }
        return 0;
    } catch (const AssemblyError& e) {
        std::cerr << "Assembly error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 2;
    }
}
#endif
