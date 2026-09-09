#include <gtest/gtest.h>
#include "PasswordHasher.h"
#include <string>

// -----------------------------------------------------------------------------
// PasswordHasher Unit Tests
// -----------------------------------------------------------------------------

TEST(PasswordHasherTest, HashDiffersFromPlaintext) {
    std::string plaintext = "MySecretPassword123!";
    std::string hash = PasswordHasher::hashPassword(plaintext);

    EXPECT_NE(hash, plaintext);
    EXPECT_FALSE(hash.empty());
}

TEST(PasswordHasherTest, EncodedValueStartsWithPbkdf2Prefix) {
    std::string plaintext = "SecurePass#2026";
    std::string hash = PasswordHasher::hashPassword(plaintext);

    EXPECT_EQ(hash.rfind("pbkdf2_sha256$", 0), 0u)
        << "Encoded hash must begin with 'pbkdf2_sha256$'";
}

TEST(PasswordHasherTest, CorrectPasswordVerifies) {
    std::string plaintext = "CorrectHorseBatteryStaple";
    std::string hash = PasswordHasher::hashPassword(plaintext);

    EXPECT_TRUE(PasswordHasher::verifyPassword(plaintext, hash));
}

TEST(PasswordHasherTest, IncorrectPasswordFails) {
    std::string plaintext = "CorrectPassword123";
    std::string hash = PasswordHasher::hashPassword(plaintext);

    EXPECT_FALSE(PasswordHasher::verifyPassword("WrongPassword456", hash));
    EXPECT_FALSE(PasswordHasher::verifyPassword("", hash));
    EXPECT_FALSE(PasswordHasher::verifyPassword("correctpassword123", hash)); // case sensitivity
}

TEST(PasswordHasherTest, SamePasswordProducesDifferentSaltedValues) {
    std::string plaintext = "IdenticalInputString!";
    std::string hash1 = PasswordHasher::hashPassword(plaintext);
    std::string hash2 = PasswordHasher::hashPassword(plaintext);

    EXPECT_NE(hash1, hash2)
        << "Random salts must ensure two hashes of the same plaintext differ";
}

TEST(PasswordHasherTest, BothSaltedValuesVerify) {
    std::string plaintext = "MultiSaltVerification99";
    std::string hash1 = PasswordHasher::hashPassword(plaintext);
    std::string hash2 = PasswordHasher::hashPassword(plaintext);

    EXPECT_TRUE(PasswordHasher::verifyPassword(plaintext, hash1));
    EXPECT_TRUE(PasswordHasher::verifyPassword(plaintext, hash2));
}

TEST(PasswordHasherTest, MalformedEncodedValuesFailSafely) {
    std::string plaintext = "TestPassword";
    std::string validHash = PasswordHasher::hashPassword(plaintext);

    // Empty string
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, ""));

    // Plaintext without delimiters
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "plaintextpassword"));

    // Missing fields (fewer than 4 parts)
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$salthex"));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000"));

    // Extra fields (more than 4 parts)
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, validHash + "$extra"));

    // Unsupported algorithm identifier
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "bcrypt$100000$0123456789abcdef0123456789abcdef$0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "sha256$100000$0123456789abcdef0123456789abcdef$0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
}

TEST(PasswordHasherTest, InvalidIterationAndHexFieldsFailSafely) {
    std::string plaintext = "TestPassword";

    // Valid 32-char salt hex and 64-char hash hex dummy placeholders
    std::string validSalt = "0123456789abcdef0123456789abcdef";
    std::string validHash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    // Non-numeric iterations
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$abc$" + validSalt + "$" + validHash));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$-100$" + validSalt + "$" + validHash));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$0$" + validSalt + "$" + validHash));

    // Iteration overflow / absurd iteration count
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$9999999999999999999999999999999$" + validSalt + "$" + validHash));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$99999999$" + validSalt + "$" + validHash));

    // Invalid salt length (too short / too long)
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$1234$" + validHash));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$" + validSalt + "aa$" + validHash));

    // Non-hex characters in salt
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz$" + validHash));

    // Invalid hash length (too short / too long)
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$" + validSalt + "$1234"));
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$" + validSalt + "$" + validHash + "aa"));

    // Non-hex characters in hash
    std::string nonHexHash = "gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg";
    EXPECT_FALSE(PasswordHasher::verifyPassword(plaintext, "pbkdf2_sha256$100000$" + validSalt + "$" + nonHexHash));
}
