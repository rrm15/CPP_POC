#ifndef PASSWORD_INPUT_H
#define PASSWORD_INPUT_H

#include <string>
#include <iostream>

// -----------------------------------------------------------------------
// PasswordInput
// -----------------------------------------------------------------------
// Provides masked ('*') password entry on Linux/POSIX terminals using
// <termios.h> (never getpass(), which is deprecated/removed on some
// platforms and offers no masking). The character-handling logic
// (insert / backspace / enter / ignore-control-chars) is factored out
// into a small, pure `applyKey` function that does not touch the
// terminal at all, specifically so it can be unit tested without a
// pseudo-terminal: tests feed it synthetic key codes and assert on the
// resulting buffer and masked output.
//
// `readMaskedPassword` is the thin I/O wrapper that actually puts the
// terminal into raw mode and drives `applyKey` from real keystrokes.
// When stdin is not a TTY (piped input, e.g. automated tests or CI),
// it transparently falls back to plain line reading instead of hanging
// or corrupting input.
// -----------------------------------------------------------------------
namespace PasswordInput {

enum class KeyAction {
    Insert,      // A printable character to append to the buffer
    Backspace,   // Backspace / Delete -- remove the last character
    Enter,       // Enter / Ctrl-D / Ctrl-C -- finish input
    Ignore       // Any other control character -- no-op
};

// Classifies a single raw character read from the terminal.
KeyAction classifyKey(char ch);

// Applies one key event to `buffer`, writing the corresponding masked
// echo (or backspace erase sequence) to `out`. Returns true if input is
// complete (Enter was processed) and false otherwise. This function has
// no dependency on termios/TTYs and is the primary unit-test target for
// the masking requirements (insertion, deletion, backspace-on-empty,
// enter behavior, control-char filtering).
bool applyKey(char ch, std::string& buffer, std::ostream& out);

// Prompts, disables terminal echo, and reads a password one keystroke
// at a time, printing '*' for each accepted character. Terminal state
// is always restored before returning (RAII guard internally), even if
// the read loop exits early (EOF, interruption). Falls back to a plain
// std::getline read when stdin is not attached to a terminal.
std::string readMaskedPassword(const std::string& prompt);

// Convenience wrapper: repeats the masked prompt until a non-empty
// password is entered. Used at every login/registration call site so
// the "field cannot be empty" policy stays consistent with the rest of
// the console UI (see InputUtil::readNonEmptyLine).
std::string readMaskedNonEmptyPassword(const std::string& prompt);

} // namespace PasswordInput

#endif // PASSWORD_INPUT_H
