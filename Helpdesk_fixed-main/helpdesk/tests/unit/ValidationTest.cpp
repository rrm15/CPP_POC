#include <gtest/gtest.h>
#include "Validation.h"
#include "Common.h"

// -----------------------------------------------------------------------
// Username validation
// -----------------------------------------------------------------------
TEST(UsernameValidationTest, AcceptsValidUsernames) {
    EXPECT_TRUE(Validation::isValidUsernameFormat("somesh"));
    EXPECT_TRUE(Validation::isValidUsernameFormat("somesh1"));
    EXPECT_TRUE(Validation::isValidUsernameFormat("somesh.km"));
    EXPECT_TRUE(Validation::isValidUsernameFormat("admin_user"));
}

TEST(UsernameValidationTest, RejectsInvalidCharacters) {
    EXPECT_FALSE(Validation::isValidUsernameFormat("somesh@"));
    EXPECT_FALSE(Validation::isValidUsernameFormat("somesh#"));
    EXPECT_FALSE(Validation::isValidUsernameFormat("somesh!"));
    EXPECT_FALSE(Validation::isValidUsernameFormat("some sh")); // space
    EXPECT_FALSE(Validation::isValidUsernameFormat("some;DROP TABLE users;"));
}

TEST(UsernameValidationTest, RejectsTooShort) {
    EXPECT_FALSE(Validation::isValidUsernameFormat(""));
    EXPECT_FALSE(Validation::isValidUsernameFormat("ab")); // 2 chars, min is 3
}

TEST(UsernameValidationTest, AcceptsMinimumLength) {
    EXPECT_TRUE(Validation::isValidUsernameFormat("abc")); // exactly 3
}

TEST(UsernameValidationTest, RejectsTooLong) {
    std::string tooLong(33, 'a');
    EXPECT_FALSE(Validation::isValidUsernameFormat(tooLong));
}

TEST(UsernameValidationTest, AcceptsMaximumLength) {
    std::string maxLen(32, 'a');
    EXPECT_TRUE(Validation::isValidUsernameFormat(maxLen));
}

TEST(UsernameValidationTest, ThrowingVariantThrowsValidationException) {
    EXPECT_THROW(Validation::validateUsernameOrThrow("bad name!"), ValidationException);
    EXPECT_NO_THROW(Validation::validateUsernameOrThrow("valid_user"));
}

// Note: case-insensitive DUPLICATE detection (somesh / Somesh / SOMESH
// colliding) is a cross-record, stateful concern and is therefore
// exercised against the real database in
// tests/integration/DuplicateConstraintsIntegrationTest.cpp rather than
// here -- this file only covers stateless format validation.

// -----------------------------------------------------------------------
// Email validation -- literal examples from the requirements
// -----------------------------------------------------------------------
TEST(EmailValidationTest, AcceptsValidEmails) {
    EXPECT_TRUE(Validation::isValidEmailFormat("somesh@example.com"));
    EXPECT_TRUE(Validation::isValidEmailFormat("somesh.km@company.com"));
    EXPECT_TRUE(Validation::isValidEmailFormat("user123@domain.org"));
    EXPECT_TRUE(Validation::isValidEmailFormat("first.last@support.net"));
}

TEST(EmailValidationTest, RejectsMissingAtSign) {
    EXPECT_FALSE(Validation::isValidEmailFormat("abc"));
    EXPECT_FALSE(Validation::isValidEmailFormat("hello"));
    EXPECT_FALSE(Validation::isValidEmailFormat("somesh.com"));
}

TEST(EmailValidationTest, RejectsMissingDomain) {
    EXPECT_FALSE(Validation::isValidEmailFormat("somesh@"));
}

TEST(EmailValidationTest, RejectsMissingLocalPart) {
    EXPECT_FALSE(Validation::isValidEmailFormat("@company.com"));
}

TEST(EmailValidationTest, RejectsMultipleAtSigns) {
    EXPECT_FALSE(Validation::isValidEmailFormat("somesh@company@company.com"));
}

TEST(EmailValidationTest, RejectsEmbeddedSpace) {
    EXPECT_FALSE(Validation::isValidEmailFormat("somesh @company.com"));
}

TEST(EmailValidationTest, RejectsEmptyString) {
    EXPECT_FALSE(Validation::isValidEmailFormat(""));
}

TEST(EmailValidationTest, RejectsDomainWithoutDot) {
    EXPECT_FALSE(Validation::isValidEmailFormat("user@localhost"));
}

TEST(EmailValidationTest, RejectsEmptyLabels) {
    EXPECT_FALSE(Validation::isValidEmailFormat("user@.com"));       // leading dot in domain
    EXPECT_FALSE(Validation::isValidEmailFormat("user@company."));   // trailing dot in domain
    EXPECT_FALSE(Validation::isValidEmailFormat("user@company..com")); // consecutive dots
    EXPECT_FALSE(Validation::isValidEmailFormat(".user@company.com")); // leading dot in local part
}

TEST(EmailValidationTest, RejectsLeadingOrTrailingWhitespace) {
    EXPECT_FALSE(Validation::isValidEmailFormat(" user@company.com"));
    EXPECT_FALSE(Validation::isValidEmailFormat("user@company.com "));
}

TEST(EmailValidationTest, ThrowingVariantThrowsValidationException) {
    EXPECT_THROW(Validation::validateEmailOrThrow("not-an-email"), ValidationException);
    EXPECT_NO_THROW(Validation::validateEmailOrThrow("valid@example.com"));
}
