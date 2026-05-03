#include "asm_highlighter.h"

#include <FL/Enumerations.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace ui {
namespace {

constexpr char kStyleInstruction = 'B';
constexpr char kStyleRegister = 'C';
constexpr char kStyleImmediate = 'D';
constexpr char kStyleLabel = 'E';
constexpr char kStyleComment = 'F';

struct AsmToken {
    std::size_t start = 0;
    std::size_t end = 0;
    std::string lower_text;
};

std::string lowercaseAscii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

bool isRegisterToken(const std::string& token) {
    return token.size() == 2 && token[0] == 'r' && token[1] >= '0' && token[1] <= '7';
}

bool hasValidDigits(const std::string& token, std::size_t begin, int base) {
    if (begin >= token.size()) {
        return false;
    }
    if (token[begin] == '-') {
        begin++;
        if (begin >= token.size()) {
            return false;
        }
    }

    for (std::size_t i = begin; i < token.size(); i++) {
        unsigned char c = static_cast<unsigned char>(token[i]);
        switch (base) {
            case 2:
                if (c != '0' && c != '1') return false;
                break;
            case 8:
                if (c < '0' || c > '7') return false;
                break;
            case 10:
                if (!std::isdigit(c)) return false;
                break;
            case 16:
                if (!std::isxdigit(c)) return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

bool isImmediateToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        return true;
    }
    switch (token[0]) {
        case '#': return hasValidDigits(token, 1, 10);
        case 'x': return hasValidDigits(token, 1, 16);
        case 'b': return hasValidDigits(token, 1, 2);
        case 'o': return hasValidDigits(token, 1, 8);
        case '-': return hasValidDigits(token, 0, 10);
        default:
            return std::isdigit(static_cast<unsigned char>(token[0])) &&
                   hasValidDigits(token, 0, 10);
    }
}

bool isInstructionToken(const std::string& token) {
    static const std::unordered_set<std::string> instructions = {
        "add", "and", "br", "brn", "brz", "brp", "brnz", "brnp", "brzp", "brnzp",
        "jmp", "jsr", "jsrr", "ld", "ldi", "ldr", "lea", "not", "ret", "rti",
        "st", "sti", "str", "trap", "getc", "out", "puts", "in", "putsp", "halt",
        ".orig", ".fill", ".stringz", ".blkw", ".end"
    };
    return instructions.find(token) != instructions.end();
}

bool isLabelToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    unsigned char first = static_cast<unsigned char>(token[0]);
    if (!(std::isalpha(first) || token[0] == '_')) {
        return false;
    }
    for (char c : token) {
        unsigned char ch = static_cast<unsigned char>(c);
        if (!(std::isalnum(ch) || c == '_')) {
            return false;
        }
    }
    return !isRegisterToken(token) && !isInstructionToken(token);
}

void applyStyle(std::string& styles, std::size_t start, std::size_t end, char style) {
    if (start >= styles.size()) {
        return;
    }
    end = std::min(end, styles.size());
    std::fill(styles.begin() + static_cast<std::ptrdiff_t>(start),
              styles.begin() + static_cast<std::ptrdiff_t>(end),
              style);
}

std::size_t findCommentStart(const std::string& text, std::size_t begin, std::size_t end) {
    bool in_string = false;
    bool escape = false;
    for (std::size_t i = begin; i < end; i++) {
        char c = text[i];
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == ';') {
            return i;
        }
    }
    return end;
}

std::vector<AsmToken> tokenizeAsmCode(const std::string& text, std::size_t begin, std::size_t end) {
    std::vector<AsmToken> tokens;
    std::size_t pos = begin;
    while (pos < end) {
        while (pos < end && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == ',')) {
            pos++;
        }
        if (pos >= end) {
            break;
        }

        std::size_t token_start = pos;
        if (text[pos] == '"') {
            pos++;
            bool escape = false;
            while (pos < end) {
                char c = text[pos++];
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    break;
                }
            }
        } else {
            while (pos < end && text[pos] != ' ' && text[pos] != '\t' &&
                   text[pos] != ',' && text[pos] != '"') {
                pos++;
            }
        }

        tokens.push_back({ token_start, pos, lowercaseAscii(text.substr(token_start, pos - token_start)) });
    }
    return tokens;
}

} // namespace

const Fl_Text_Display::Style_Table_Entry kAsmStyleTable[] = {
    { FL_BLACK, FL_COURIER, 14, 0, 0 },
    { fl_rgb_color(20, 74, 145), FL_COURIER_BOLD, 14, 0, 0 },
    { fl_rgb_color(174, 43, 43), FL_COURIER_BOLD, 14, 0, 0 },
    { fl_rgb_color(30, 122, 67), FL_COURIER, 14, 0, 0 },
    { fl_rgb_color(116, 70, 168), FL_COURIER, 14, 0, 0 },
    { fl_rgb_color(113, 119, 124), FL_COURIER_ITALIC, 14, 0, 0 },
};

const int kAsmStyleTableSize =
    static_cast<int>(sizeof(kAsmStyleTable) / sizeof(kAsmStyleTable[0]));

std::string buildAsmStyleText(const std::string& text) {
    std::string styles(text.size(), kAsmStyleDefault);
    std::size_t line_begin = 0;
    while (line_begin < text.size()) {
        std::size_t line_end = text.find('\n', line_begin);
        if (line_end == std::string::npos) {
            line_end = text.size();
        }

        std::size_t comment_start = findCommentStart(text, line_begin, line_end);
        if (comment_start < line_end) {
            applyStyle(styles, comment_start, line_end, kStyleComment);
        }

        std::vector<AsmToken> tokens = tokenizeAsmCode(text, line_begin, comment_start);
        for (const AsmToken& token : tokens) {
            char style = kAsmStyleDefault;
            if (isInstructionToken(token.lower_text)) {
                style = kStyleInstruction;
            } else if (isRegisterToken(token.lower_text)) {
                style = kStyleRegister;
            } else if (isImmediateToken(token.lower_text)) {
                style = kStyleImmediate;
            } else if (isLabelToken(token.lower_text)) {
                style = kStyleLabel;
            }
            applyStyle(styles, token.start, token.end, style);
        }

        line_begin = (line_end < text.size()) ? line_end + 1 : text.size();
    }
    return styles;
}

} // namespace ui
