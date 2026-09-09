#include <gtest/gtest.h>
#include <sstream>
#include "PasswordInput.h"

using PasswordInput::KeyAction;
using PasswordInput::classifyKey;
using PasswordInput::applyKey;

// -----------------------------------------------------------------------
// classifyKey -- pure classification, no state
// -----------------------------------------------------------------------
TEST(PasswordInputClassifyTest, PrintableCharsAreInsert) {
    EXPECT_EQ(classifyKey('a'), KeyAction::Insert);
    EXPECT_EQ(classifyKey('Z'), KeyAction::Insert);
    EXPECT_EQ(classifyKey('5'), KeyAction::Insert);
    EXPECT_EQ(classifyKey('!'), KeyAction::Insert);
    EXPECT_EQ(classifyKey(' '), KeyAction::Insert);
}

TEST(PasswordInputClassifyTest, EnterKeysAreEnter) {
    EXPECT_EQ(classifyKey('\n'), KeyAction::Enter);
    EXPECT_EQ(classifyKey('\r'), KeyAction::Enter);
}

TEST(PasswordInputClassifyTest, BackspaceAndDeleteAreBackspace) {
    EXPECT_EQ(classifyKey(127), KeyAction::Backspace);
    EXPECT_EQ(classifyKey(8), KeyAction::Backspace);
}

TEST(PasswordInputClassifyTest, ControlCharsOtherThanNewlineAreIgnoredOrEnter) {
    EXPECT_EQ(classifyKey(3), KeyAction::Enter);   // Ctrl-C: treated as "finish safely"
    EXPECT_EQ(classifyKey(4), KeyAction::Enter);   // Ctrl-D / EOF: treated as "finish safely"
    EXPECT_EQ(classifyKey(1), KeyAction::Ignore);  // Ctrl-A: not a recognized action
    EXPECT_EQ(classifyKey(27), KeyAction::Ignore); // Escape
}

// -----------------------------------------------------------------------
// applyKey -- character insertion
// -----------------------------------------------------------------------
TEST(PasswordInputApplyKeyTest, InsertingCharactersAppendsToBufferAndPrintsMask) {
    std::string buffer;
    std::ostringstream out;

    EXPECT_FALSE(applyKey('p', buffer, out));
    EXPECT_FALSE(applyKey('a', buffer, out));
    EXPECT_FALSE(applyKey('s', buffer, out));
    EXPECT_FALSE(applyKey('s', buffer, out));

    EXPECT_EQ(buffer, "pass");
    EXPECT_EQ(out.str(), "****"); // one '*' printed per accepted character
}

// -----------------------------------------------------------------------
// applyKey -- character deletion / backspace logic
// -----------------------------------------------------------------------
TEST(PasswordInputApplyKeyTest, BackspaceRemovesLastCharacterFromBuffer) {
    std::string buffer = "abc";
    std::ostringstream out;

    applyKey(static_cast<char>(127), buffer, out);

    EXPECT_EQ(buffer, "ab");
}

TEST(PasswordInputApplyKeyTest, BackspaceWritesEraseSequenceToScreen) {
    std::string buffer = "x";
    std::ostringstream out;

    applyKey(static_cast<char>(127), buffer, out);

    EXPECT_EQ(out.str(), "\b \b"); // move back, blank the '*', move back again
}

TEST(PasswordInputApplyKeyTest, BackspaceOnEmptyBufferIsANoOp) {
    std::string buffer; // already empty
    std::ostringstream out;

    bool done = applyKey(static_cast<char>(127), buffer, out);

    EXPECT_FALSE(done);
    EXPECT_TRUE(buffer.empty());
    EXPECT_TRUE(out.str().empty()) << "nothing should be erased from the screen when there is nothing typed";
}

TEST(PasswordInputApplyKeyTest, InsertThenBackspaceThenInsertProducesCorrectBuffer) {
    std::string buffer;
    std::ostringstream out;

    applyKey('h', buffer, out);
    applyKey('i', buffer, out);
    applyKey(static_cast<char>(8), buffer, out); // backspace (BS variant)
    applyKey('o', buffer, out);

    EXPECT_EQ(buffer, "ho");
}

// -----------------------------------------------------------------------
// applyKey -- Enter behavior
// -----------------------------------------------------------------------
TEST(PasswordInputApplyKeyTest, EnterFinalizesInputWithoutModifyingBuffer) {
    std::string buffer = "secret";
    std::ostringstream out;

    bool done = applyKey('\n', buffer, out);

    EXPECT_TRUE(done);
    EXPECT_EQ(buffer, "secret");
    EXPECT_EQ(out.str(), "\n");
}

TEST(PasswordInputApplyKeyTest, EnterOnEmptyBufferYieldsEmptyPassword) {
    std::string buffer;
    std::ostringstream out;

    bool done = applyKey('\n', buffer, out);

    EXPECT_TRUE(done);
    EXPECT_TRUE(buffer.empty());
}

// -----------------------------------------------------------------------
// applyKey -- ignored control characters
// -----------------------------------------------------------------------
TEST(PasswordInputApplyKeyTest, UnrecognizedControlCharIsIgnoredCompletely) {
    std::string buffer = "ab";
    std::ostringstream out;

    bool done = applyKey(static_cast<char>(1), buffer, out); // Ctrl-A

    EXPECT_FALSE(done);
    EXPECT_EQ(buffer, "ab");
    EXPECT_TRUE(out.str().empty());
}

// -----------------------------------------------------------------------
// readMaskedPassword -- non-TTY fallback path
// -----------------------------------------------------------------------
// When stdin is not attached to a terminal (exactly the situation under
// which this test binary itself runs via ctest/CI), readMaskedPassword
// falls back to a plain std::getline read rather than attempting
// termios raw-mode I/O. This exercises that fallback end-to-end,
// including the empty-password / EOF case.
TEST(PasswordInputReadTest, NonTtyFallbackReadsWholeLine) {
    std::istringstream fakeStdin("mypassword\n");
    std::streambuf* originalBuf = std::cin.rdbuf(fakeStdin.rdbuf());

    std::string result = PasswordInput::readMaskedPassword("Password: ");

    std::cin.rdbuf(originalBuf);
    EXPECT_EQ(result, "mypassword");
}

TEST(PasswordInputReadTest, NonTtyFallbackOnEmptyStreamReturnsEmptyString) {
    std::istringstream fakeStdin(""); // simulates EOF with nothing typed
    std::streambuf* originalBuf = std::cin.rdbuf(fakeStdin.rdbuf());

    std::string result = PasswordInput::readMaskedPassword("Password: ");

    std::cin.rdbuf(originalBuf);
    EXPECT_TRUE(result.empty());
}

TEST(PasswordInputReadTest, NonEmptyWrapperRePromptsUntilNonEmptyInput) {
    // First line is empty (simulating the user hitting Enter with no
    // input), second line has the real password.
    std::istringstream fakeStdin("\nrealpassword\n");
    std::streambuf* originalBuf = std::cin.rdbuf(fakeStdin.rdbuf());

    std::string result = PasswordInput::readMaskedNonEmptyPassword("Password: ");

    std::cin.rdbuf(originalBuf);
    EXPECT_EQ(result, "realpassword");
}
