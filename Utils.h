#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

// Small helpers shared by every module of CodeQuest.
namespace util {

enum class Tone { Cyan, Blue, Magenta, Green, Yellow, Red, Muted, Bold };

inline bool& terminalStyles() {
    static bool enabled = false;
    return enabled;
}

inline void enableTerminalStyles() {
    bool interactive = false;
#ifdef _WIN32
    interactive = _isatty(_fileno(stdout)) != 0;
    if (interactive) {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (output == INVALID_HANDLE_VALUE || !GetConsoleMode(output, &mode) ||
            !SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            interactive = false;
        }
    }
#else
    interactive = isatty(STDOUT_FILENO) != 0;
#endif
    terminalStyles() = interactive && std::getenv("NO_COLOR") == nullptr;
}

inline std::string paint(const std::string& text, Tone tone) {
    if (!terminalStyles()) return text;
    const char* code = "";
    switch (tone) {
        case Tone::Cyan: code = "\033[36m"; break;
        case Tone::Blue: code = "\033[34m"; break;
        case Tone::Magenta: code = "\033[35m"; break;
        case Tone::Green: code = "\033[32m"; break;
        case Tone::Yellow: code = "\033[33m"; break;
        case Tone::Red: code = "\033[31m"; break;
        case Tone::Muted: code = "\033[90m"; break;
        case Tone::Bold: code = "\033[1m"; break;
    }
    return std::string(code) + text + "\033[0m";
}

// Thrown when stdin is closed (Ctrl+D / Ctrl+Z) so the game can save and exit cleanly.
struct InputClosed : std::runtime_error {
    InputClosed() : std::runtime_error("input closed") {}
};

inline std::string trim(const std::string& s) {
    const std::string ws = " \t\r\n";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

inline std::string repeat(char c, int n) { return std::string(n > 0 ? n : 0, c); }

inline std::string readLine(const std::string& prompt) {
    std::cout << paint(prompt, Tone::Cyan) << std::flush;
    std::string s;
    if (!std::getline(std::cin, s)) throw InputClosed();
    return trim(s);
}

inline int readInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        const std::string s = readLine(prompt);
        try {
            std::size_t pos = 0;
            const int v = std::stoi(s, &pos);
            if (pos == s.size() && v >= lo && v <= hi) return v;
        } catch (...) {
        }
        std::cout << paint("  Please enter a number between " + std::to_string(lo) + " and " +
                               std::to_string(hi) + ".\n",
                           Tone::Red);
    }
}

inline void banner(const std::string& title) {
    const int width = std::max(52, static_cast<int>(title.size()) + 8);
    const std::string edge = "+" + repeat('-', width - 2) + "+";
    const int pad = std::max(0, (width - 2 - static_cast<int>(title.size())) / 2);
    std::cout << "\n" << paint(edge, Tone::Cyan) << "\n"
              << paint("|", Tone::Cyan) << repeat(' ', pad)
              << paint(title, Tone::Bold) << repeat(' ', width - 2 - pad -
                                                        static_cast<int>(title.size()))
              << paint("|", Tone::Cyan) << "\n"
              << paint(edge, Tone::Cyan) << "\n";
}

inline void pause() { readLine("\nPress Enter to continue..."); }

}  // namespace util

#endif
