#include <bits/stdc++.h>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

class Configuration {
    // some configuration
public:
    static bool OutputBitCnt; // whether output the bit counter to output file
    static bool TraceWord; // whether print instruction info to console
    static bool TraceLabel; // whether print label info to console
    static bool OutputHex; // whether output hex format to file
    static bool OutputX3000; // whether add '0x3000' to the beginning of output file
    static bool InputFromConsole;
    static bool OutputToConsole;
};

bool Configuration::OutputBitCnt = false;
bool Configuration::TraceLabel = true;
bool Configuration::TraceWord = true;
bool Configuration::OutputHex = false;
bool Configuration::OutputX3000 = true;
bool Configuration::InputFromConsole = false;
bool Configuration::OutputToConsole = false;

class Tokenizer {
    static vector<string> SplitLines(const string &text) {
        // split the input string to lines
        vector<string> lines;
        istringstream iss(text);
        string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        return lines;
    }

    static vector<string> TokenizeLine(const string &line) {
        // tokenize a line
        vector<string> tokens;
        string current_token;
        bool in_string = false;
        bool escape = false; //whether meet a '\' escape character

        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];

            if (in_string) {
                if (escape) {
                    // for character\ ("\\") and quote mark" ("\"") in string
                    if (c == '"' || c == '\\') {
                        current_token += c;
                        escape = false;
                    } else {
                        current_token += '\\';
                        current_token += c;
                        escape = false;
                    }
                } else {
                    if (c == '\\') {
                        escape = true;
                    } else if (c == '"') {
                        current_token += c;
                        tokens.push_back(current_token);
                        current_token.clear();
                        in_string = false;
                    } else {
                        current_token += c;
                    }
                }
            } else {
                if (c == ';') {
                    // ignore comments
                    break;
                } else if (c == '"') {
                    if (!current_token.empty()) {
                        tokens.push_back(current_token);
                        current_token.clear();
                    }
                    current_token += c;
                    in_string = true;
                    escape = false;
                } else if (c == ' ' || c == ',') {
                    if (!current_token.empty()) {
                        tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
            }
        }

        if (!current_token.empty()) {
            tokens.push_back(current_token);
        }
        return tokens;
    }

    static string ReadFileIntoString(const string &filename) {
        ifstream file(filename, ios::binary);
        if (!file)
            assert(false);

        ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

public:
    static vector<string> Tokenize(const string &path) {
        // tokenize content from a file
        string content;
        if (Configuration::InputFromConsole) {
            string line;
            while (getline(cin, line)) {
                if (line == ".end" || line == ".END") {
                    break;
                }
                content += line;
                content += "\n";
            }
        } else {
            content = ReadFileIntoString(path);
        }
        auto lines = SplitLines(content);
        vector<string> result;
        for (auto &i: lines) {
            auto thisLine = TokenizeLine(i);
            result.insert(result.end(), thisLine.begin(), thisLine.end());
        }
        return result;
    }

    static vector<string> Convert2Lowercase(const vector<string> &src) {
        // convert all uppercase character to lowercase
        vector<string> result;
        for (auto &word: src) {
            string lowercase_word = word;
            if (lowercase_word.length() >= 2) {
                if (lowercase_word[0] == '\"' && lowercase_word[lowercase_word.length() - 1] == '\"') {
                    result.push_back(lowercase_word);
                    continue;
                }
            }
            for (int i = 0; i < lowercase_word.length(); i++) {
                if (lowercase_word[i] >= 'A' && lowercase_word[i] <= 'Z') {
                    lowercase_word[i] -= 'A';
                    lowercase_word[i] += 'a';
                }
            }
            result.push_back(lowercase_word);
        }
        return result;
    }
};

class Parser {
    map<string, string> valid_token; //store instruction->01string map (e.g. br->"0000")
    map<string, int> label_line; // store label->line map

public:
    Parser() {
        // read configuration from file
        // .json file contains information such as
        // "br"->"0000", "r6"->"110"
        string file_path = "config.json";
        ifstream in(file_path);
        if (!in)
            assert(false);
        json j;
        in >> j;
        for (auto &[k, v]: j.items()) {
            if (!v.is_string())
                assert(false);
            valid_token.emplace(k, v.get<string>());
            // cout << k << " " << v.get<string>() << endl;
        }
    }

    [[nodiscard]] static bool isReg(const string &s) {
        // check whether a token refers to a register
        if (s.length() == 2 && s[0] == 'r' && s[1] <= '7' && s[1] >= '0') return true;
        return false;
    }

    [[nodiscard]] bool isPseudoInst(const string &s) const {
        // check whether a token refers to a pseudo instruction such as .blkw .orig
        if (valid_token.contains(s) == false) {
            return false;
        }
        if (s.length() > 1 && s[0] == '.') {
            return true;
        }
        return false;
    }

    [[nodiscard]] bool isSyntax(const string &s) const {
        // check whether a token refers to a instruction such as add, brnz
        if (valid_token.contains(s) == false) {
            return false;
        }
        if (isReg(s) || isPseudoInst(s)) {
            return false;
        }
        return true;
    }

    [[nodiscard]] static bool isString(const string &s) {
        // check whether a token refers to a string
        if (s.length() >= 2 && s.front() == '\"' && s.back() == '\"') {
            return true;
        }
        return false;
    }

    [[nodiscard]] static bool isValidImmediateNum(const string &s) {
        // check whether a token refers to a immediate number
        enum DECIMAL {
            HEX, DEC, BIN, OCT
        } type;
        type = DEC;
        int begin = 0;
        if (s[0] == '#' || s[0] == 'x' || s[0] == 'b' || s[0] == 'o') {
            if (s[0] == '#') type = DEC;
            if (s[0] == 'x') type = HEX;
            if (s[0] == 'o') type = OCT;
            if (s[0] == 'b') type = BIN;
            begin++;
            if (s[1] == '-') begin++;
        } else if (isdigit(s[0])) {
        } else if (isdigit(s[0] == '-')) {
            begin++;
        } else {
            return false;
        }
        for (int i = begin; i < s.length(); i++) {
            if (type == OCT) {
                if (!(s[i] >= '0' && s[i] <= '7')) return false;
            } else if (type == BIN) {
                if (!(s[i] >= '0' && s[i] <= '1')) return false;
            } else if (type == HEX) {
                if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
            } else {
                if (!(s[i] >= '0' && s[i] <= '9')) return false;
            }
        }
        return true;
    }

    [[nodiscard]] static bool isTrap(const string &s) {
        // check whether a token refers to a trap
        return s == "getc" || s == "out" || s == "puts" || s == "in" || s == "putsp" || s == "halt";
    }

    [[nodiscard]] bool isLabel(const string &s) const {
        // check whether a token refers to a label
        return !isReg(s) && !isSyntax(s) && !isPseudoInst(s) && !isValidImmediateNum(s) && !isString(s) && !isTrap(s);
    }

    static string GetImmediateNumStr(const string &s, const int digit = 16) {
        //convert an immediate number string to 01 string for this number
        if (isValidImmediateNum(s) == false)
            assert(false);
        int base = 10;
        string num_str = s;
        if (s[0] == '#') {
            base = 10;
            num_str = s.substr(1);
        } else if (s[0] == 'x') {
            base = 16;
            num_str = s.substr(1);
        } else if (s[0] == 'o') {
            base = 8;
            num_str = s.substr(1);
        } else if (s[0] == 'b') {
            base = 2;
            num_str = s.substr(1);
        } else if (isdigit(s[0])) {
            num_str = s;
        } else {
            assert(false);
        }

        long long value;
        try {
            value = stoll(num_str, nullptr, base);
        } catch (...) {
            assert(false);
        }

        const long long min_val = -(1LL << (digit - 1));
        const long long max_val = (1LL << (digit - 1)) - 1;
        //TODO check whether the immediate value is overflow or underflow ...

        // if (value < min_val || value > max_val) {
        //     assert(false);
        // }

        unsigned long long uvalue; // complement value
        if (value < 0) {
            uvalue = (1ULL << digit) + value;
        } else {
            uvalue = value;
        }

        bitset<64> bits(uvalue);
        string result = bits.to_string().substr(64 - digit);
        return result;
    }

    static string GetImmediateNumStr(const int value, const int digit = 16) {
        //convert an immediate number integer to 01 string

        //TODO check whether the immediate value is overflow or underflow ...

        // long long min_val = -(1LL << (digit - 1));
        // long long max_val = (1LL << (digit - 1)) - 1;
        //
        // if (value < min_val || value > max_val) {
        //     assert(false);
        // }

        unsigned long long uvalue; // complement value
        if (value < 0) {
            uvalue = (1ULL << digit) + value;
        } else {
            uvalue = value;
        }

        bitset<64> bits(uvalue);
        string result = bits.to_string().substr(64 - digit); //capture the requested digits
        return result;
    }

    static int GetImmediateNum(const string &s, const int digit = 16) {
        // convert an immediate number string to integer
        if (isValidImmediateNum(s) == false)
            assert(false);
        int base = 10;
        string num_str = s;
        if (s[0] == '#') {
            base = 10;
            num_str = s.substr(1);
        } else if (s[0] == 'x') {
            base = 16;
            num_str = s.substr(1);
        } else if (s[0] == 'o') {
            base = 8;
            num_str = s.substr(1);
        } else if (s[0] == 'b') {
            base = 2;
            num_str = s.substr(1);
        } else if (isdigit(s[0])) {
            num_str = s;
        } else {
            assert(false);
        }

        int value;
        try {
            value = stoi(num_str, nullptr, base);
        } catch (...) {
            assert(false);
        }

        return value;
    }

    static string GetRealString(const string &str) {
        // receive a string which denotes a string, e.g. ""a string""(a string begins and ends with a quote mark)
        // output the length of the inner string, e.g. the length of "a string", which is 8
        if (str.length() < 2 || str.front() != '\"' || str.back() != '\"') {
            assert(false);
        }
        return str.substr(1, str.length() - 2);
    }


    string ParserFromFile(string &path) {
        // assemble code!
        const auto split_upper = Tokenizer::Tokenize(path);
        const auto split = Tokenizer::Convert2Lowercase(split_upper);
        string result;

        int line = 0;
        // memorize which line the current token is on
        // (in the 01 instruction file, not in the assembly language code file)

        //the first scan: find label and check whether the assembly code is legal
        for (int i = 0; i < split.size(); i++) {
            const auto &word = split[i];

            if (isTrap(word)) {
                if (valid_token.contains(word) == false) {
                    assert(false);
                }
                line++;
                continue;
            }

            if (isPseudoInst(word)) {
                //TODO multiple code pieces
                if (word == ".orig") {
                    if (split.size() - i - 1 < 1)
                        assert(false);
                    if (isValidImmediateNum(split[i + 1]) == false)
                        assert(false);
                    i++;
                } else if (word == ".blkw") {
                    if (split.size() - i - 1 < 1)
                        assert(false);
                    if (isValidImmediateNum(split[i + 1]) == false)
                        assert(false);
                    auto imme = GetImmediateNum(split[i + 1], 16);
                    line += imme;
                    i++;
                } else if (word == ".stringz") {
                    if (split.size() - i - 1 < 1)
                        assert(false);
                    if (isString(split[i + 1]) == false)
                        assert(false);

                    const auto &str_with_quotemark = split[i + 1];
                    line += str_with_quotemark.length() - 1;
                    i++;
                } else if (word == ".fill") {
                    if (split.size() - i - 1 < 1)
                        assert(false);
                    if (isValidImmediateNum(split[i + 1]) == false)
                        assert(false);
                    line++;
                    i++;
                } else if (word == ".end") {
                }
                continue;
            }

            if (isLabel(word)) {
                if (label_line.contains(word))
                    assert(false);
                label_line.emplace(word, line);
                if (Configuration::TraceLabel) {
                    cout << "label " << word << " on line" << hex << line << endl;
                }
                continue;
            }

            if (valid_token.contains(word) == false)
                assert(false);

            if (word == "add" || word == "and") {
                if (split.size() - i - 1 < 3)
                    assert(false);
                if (!isReg(split[i + 1]))
                    assert(false);
                if (!isReg(split[i + 2]))
                    assert(false);
                i += 3;
                line++;
                continue;
            }

            if (word == "br" || word == "brn" || word == "brz" || word == "brp" ||
                word == "brnz" || word == "brnp" || word == "brzp" || word == "brnzp") {
                if (split.size() - i - 1 < 1)
                    assert(false);
                if (isLabel(split[i + 1]) == false)
                    assert(false);
                i++;
                line++;
                continue;
            }

            if (word == "jmp" || word == "jsrr") {
                if (split.size() - i - 1 < 1)
                    assert(false);
                if (isReg(split[i + 1]) == false)
                    assert(false);
                i++;
                line++;
                continue;
            }

            if (word == "jsr") {
                if (split.size() - i - 1 < 1)
                    assert(false);
                if (isLabel(split[i + 1]) == false)
                    assert(false);
                i++;
                line++;
                continue;
            }

            if (word == "ld" || word == "st" || word == "ldi" || word == "sti" || word == "lea") {
                if (split.size() - i - 1 < 2)
                    assert(false);
                if (isReg(split[i + 1]) == false)
                    assert(false);
                if (isLabel(split[i + 2]) == false)
                    assert(false);
                i += 2;
                line++;
                continue;
            }

            if (word == "ldr" || word == "str") {
                if (split.size() - i - 1 < 3)
                    assert(false);
                if (!isReg(split[i + 1]))
                    assert(false);
                if (!isReg(split[i + 2]))
                    assert(false);
                i += 3;
                line++;
                continue;
            }

            if (word == "not") {
                if (split.size() - i - 1 < 2)
                    assert(false);
                if (!isReg(split[i + 1]))
                    assert(false);
                if (!isReg(split[i + 2]))
                    assert(false);
                i += 2;
                line++;
                continue;
            }

            if (word == "ret") {
                line++;
                continue;
            }

            if (word == "rti") {
                line++;
                continue;
            }

            if (word == "trap") {
                if (split.size() - i - 1 < 1)
                    assert(false);
                i++;
                line++;
                continue;
            }
        }

        line = 0;
        //the second scan, assemble code to instruction
        for (int i = 0; i < split.size(); i++) {
            assert(result.length()%16==0);

            const auto &word = split[i];
            if (Configuration::TraceWord) {
                cout << "word " << word << " on line" << line << endl;
            }

            if (isTrap(word)) {
                result += valid_token[word];
                line++;
                continue;
            }

            if (isPseudoInst(word)) {
                //TODO multiple code pieces
                if (word == ".orig") {
                    i++;
                } else if (word == ".blkw") {
                    auto imme = GetImmediateNum(split[i + 1]);
                    for (int j = 0; j < imme; j++) {
                        result += "0000000000000000";
                    }
                    i++;
                    line += imme;
                } else if (word == ".stringz") {
                    const auto &str_with_quoteMark = split[i + 1];
                    for (auto str = GetRealString(str_with_quoteMark); char c: str) {
                        auto char_to_complement_bits_str = GetImmediateNumStr(c, 16);
                        assert(char_to_complement_bits_str.length()==16);
                        result += char_to_complement_bits_str;
                    }
                    result += "0000000000000000";
                    i++;
                    line += str_with_quoteMark.length() - 1;
                } else if (word == ".fill") {
                    auto imme = GetImmediateNumStr(split[i + 1], 16);
                    assert(imme.length()==16);
                    result += imme;
                    i++;
                    line++;
                } else if (word == ".end") {
                }
                continue;
            }

            if (isLabel(word)) {
                continue;
            }

            result += valid_token[word];
            if (word == "add" || word == "and") {
                result += valid_token[split[i + 1]];
                result += valid_token[split[i + 2]];

                if (isReg(split[i + 3])) {
                    result += "000";
                    result += valid_token[split[i + 3]];
                } else {
                    result += '1';
                    result += GetImmediateNumStr(split[i + 3], 5);
                }

                i += 3;
                line++;
                continue;
            }

            if (word == "brn" || word == "brz" || word == "brp" ||
                word == "brnz" || word == "brnp" || word == "brzp" || word == "brnzp") {
                if (word.find_first_of('n') != std::string::npos) {
                    result += "1";
                } else {
                    result += "0";
                }
                if (word.find_first_of('z') != std::string::npos) {
                    result += "1";
                } else {
                    result += "0";
                }
                if (word.find_first_of('p') != std::string::npos) {
                    result += "1";
                } else {
                    result += "0";
                }

                if (label_line.contains(split[i + 1]) == false) {
                    assert(false);
                }
                int PCoffset = label_line[split[i + 1]] - line - 1;
                result += GetImmediateNumStr(PCoffset, 9);
                i++;
                line++;
                continue;
            }

            if (word == "br") {
                result += "111";
                if (label_line.contains(split[i + 1]) == false) {
                    assert(false);
                }
                int PCoffset = label_line[split[i + 1]] - line - 1;
                result += GetImmediateNumStr(PCoffset, 9);
                i++;
                line++;
                continue;
            }

            if (word == "jmp" || word == "jsrr") {
                result += "000";
                result += valid_token[split[i + 1]];
                result += "000000";

                i++;
                line++;
                continue;
            }

            if (word == "jsr") {
                result += "1";
                int PCoffset = label_line[split[i + 1]] - line - 1;
                result += GetImmediateNumStr(PCoffset, 11);
                i++;
                line++;
                continue;
            }

            if (word == "ld" || word == "st" || word == "ldi" || word == "sti" || word == "lea") {
                result += valid_token[split[i + 1]];
                int PCoffset = label_line[split[i + 2]] - line - 1;
                result += GetImmediateNumStr(PCoffset, 9);
                i += 2;
                line++;
                continue;
            }

            if (word == "ldr" || word == "str") {
                result += valid_token[split[i + 1]];
                result += valid_token[split[i + 2]];
                result += GetImmediateNumStr(split[i + 3], 6);

                i += 3;
                line++;
                continue;
            }

            if (word == "not") {
                result += valid_token[split[i + 1]];
                result += valid_token[split[i + 2]];
                result += "111111";

                i += 2;
                line++;
                continue;
            }

            if (word == "ret") {
                result += "000111000000";
                line++;
                continue;
            }

            if (word == "rti") {
                result += "000000000000";
                line++;
                continue;
            }

            if (word == "trap") {
                result += "0000";
                result += GetImmediateNumStr(split[i + 1], 8);

                i++;
                line++;
                continue;
            }

            assert(false);
        }

        return result;
    }
};

class Utilities {
public:
    static string BinStr2HexStr(const string &str) {
        // convert a binary string to hex string
        assert(str.length()==16);
        string result;
        for (int i = 0; i < 4; i++) {
            auto sub = str.substr(i * 4, 4);
            if (sub == "0000") {
                result += '0';
            } else if (sub == "0001") {
                result += '1';
            } else if (sub == "0010") {
                result += '2';
            } else if (sub == "0011") {
                result += '3';
            } else if (sub == "0100") {
                result += '4';
            } else if (sub == "0101") {
                result += '5';
            } else if (sub == "0110") {
                result += '6';
            } else if (sub == "0111") {
                result += '7';
            } else if (sub == "1000") {
                result += '8';
            } else if (sub == "1001") {
                result += '9';
            } else if (sub == "1010") {
                result += 'A';
            } else if (sub == "1011") {
                result += 'B';
            } else if (sub == "1100") {
                result += 'C';
            } else if (sub == "1101") {
                result += 'D';
            } else if (sub == "1110") {
                result += 'E';
            } else if (sub == "1111") {
                result += 'F';
            } else {
                assert(false);
            }
        }
        return result;
    }
};

int main1() {
    // I don't know how to write cmake, so name the unused main function as main1
    Parser p;
    string input_path = "input.txt";
    const auto result = p.ParserFromFile(input_path);
    assert(result.length()%16==0);

    if (Configuration::OutputToConsole == false) {
        ofstream outFile("output.txt");
        assert(outFile);
        streambuf *oldCoutBuffer = std::cout.rdbuf();
        cout.rdbuf(outFile.rdbuf());

        if (Configuration::OutputX3000) {
            cout << "0011000000000000" << endl;
        }
        if (Configuration::OutputBitCnt) {
            cout << result.length() << endl;
        }
        for (int i = 0; i < result.length(); i++) {
            auto line = result.substr(i, 16);
            cout << line;
            if (Configuration::OutputHex) {
                auto hex = Utilities::BinStr2HexStr(line);
                cout << "  ;x" << hex;
            }
            cout << endl;
            i += 15;
        }
        cout.rdbuf(oldCoutBuffer);
    } else {
        if (Configuration::OutputX3000) {
            cout << "0011000000000000" << endl;
        }
        if (Configuration::OutputBitCnt) {
            cout << result.length() << endl;
        }
        for (int i = 0; i < result.length(); i++) {
            auto line = result.substr(i, 16);
            cout << line;
            if (Configuration::OutputHex) {
                auto hex = Utilities::BinStr2HexStr(line);
                cout << "  ;x" << hex;
            }
            cout << endl;
            i += 15;
        }
    }
    return 0;
}
