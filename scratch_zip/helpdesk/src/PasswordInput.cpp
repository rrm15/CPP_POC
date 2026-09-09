#include "PasswordInput.h"

#include <iostream>

#if defined(__unix__) || defined(__APPLE__)
#define HELPDESK_HAS_TERMIOS 1
#include <termios.h>
#include <unistd.h>
#endif

namespace {

#ifdef HELPDESK_HAS_TERMIOS
// RAII guard: switches the controlling terminal into "raw-ish" mode
// (echo off, canonical mode off, signal generation off so Ctrl-C is
// delivered as a normal byte instead of killing the process) for as
// long as the guard is alive, and unconditionally restores the
// original settings when it goes out of scope -- covering every exit
// path out of readMaskedPassword (normal return, early `break`, or an
// exception unwinding through the function).
class TerminalRawGuard {
public:
    TerminalRawGuard() {
        if (tcgetattr(STDIN_FILENO, &original_) == 0) {
            termios raw = original_;
            raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | ISIG));
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
                active_ = true;
            }
        }
    }

    ~TerminalRawGuard() {
        if (active_) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
        }
    }

    TerminalRawGuard(const TerminalRawGuard&) = delete;
    TerminalRawGuard& operator=(const TerminalRawGuard&) = delete;

    bool isActive() const { return active_; }

private:
    termios original_{};
    bool active_ = false;
};
#endif

} // namespace

namespace PasswordInput {

KeyAction classifyKey(char ch) {
    if (ch == '\n' || ch == '\r') return KeyAction::Enter;
    if (ch == 4 /* Ctrl-D / EOF */ || ch == 3 /* Ctrl-C */) return KeyAction::Enter;
    if (ch == 127 /* Delete */ || ch == 8 /* Backspace */) return KeyAction::Backspace;
    if (static_cast<unsigned char>(ch) >= 32 && static_cast<unsigned char>(ch) < 127) {
        return KeyAction::Insert;
    }
    return KeyAction::Ignore;
}

bool applyKey(char ch, std::string& buffer, std::ostream& out) {
    switch (classifyKey(ch)) {
        case KeyAction::Enter:
            out << "\n";
            return true;
        case KeyAction::Backspace:
            if (!buffer.empty()) {
                buffer.pop_back();
                // Move cursor back, overwrite the '*' with a space, move
                // back again -- the standard terminal "erase one char"
                // sequence. No-op (nothing printed) when buffer is
                // already empty, since there is nothing on screen to erase.
                out << "\b \b";
            }
            return false;
        case KeyAction::Insert:
            buffer.push_back(ch);
            out << '*';
            return false;
        case KeyAction::Ignore:
        default:
            return false;
    }
}

std::string readMaskedPassword(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

#ifdef HELPDESK_HAS_TERMIOS
    if (isatty(STDIN_FILENO)) {
        TerminalRawGuard guard;
        if (guard.isActive()) {
            std::string password;
            char ch = 0;
            while (true) {
                // Read through std::cin rather than calling read() on the
                // raw fd directly. tcgetattr/tcsetattr above still operate
                // on the real STDIN_FILENO, so raw-mode (no echo, no
                // canonical buffering) is genuinely in effect on the
                // terminal -- but the *character source* is now whatever
                // streambuf std::cin has attached at the time. In normal
                // interactive use that's still the real terminal (via the
                // standard C library's buffering on top of fd 0), so
                // behavior for a real user is unchanged. In tests, where
                // std::cin.rdbuf() has been redirected to a fake
                // std::istringstream to script input, this now actually
                // consumes that scripted input instead of silently
                // ignoring it and blocking on real keystrokes.
                if (!std::cin.get(ch)) break; // EOF / stream failure: stop, guard restores terminal below
                if (applyKey(ch, password, std::cout)) break;
                std::cout.flush();
            }
            return password;
        }
        // Terminal exists but we could not switch it into raw mode
        // (e.g. permissions); fall through to the plain-read fallback
        // below rather than failing the whole login/registration flow.
    }
#endif

    // Non-interactive stdin (piped input, redirected file, unit/
    // integration tests run under CI without a controlling terminal) or
    // no termios support on this platform: masking a TTY-only feature
    // is meaningless here, so just read a line normally.
    std::string line;
    std::getline(std::cin, line);
    return line;
}

std::string readMaskedNonEmptyPassword(const std::string& prompt) {
    while (true) {
        std::string password = readMaskedPassword(prompt);
        if (!password.empty()) return password;
        std::cout << "Password cannot be empty. Please try again.\n";
    }
}

} // namespace PasswordInput
