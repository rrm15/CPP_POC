#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Common.h"
#include "PasswordHasher.h"

// Scenario: Create User -> Login Success -> Login Failure
// Uses a dedicated temporary SQLite file (never the production DB),
// created fresh and deleted by TestDatabaseFixture.
TEST_F(TestDatabaseFixture, AuthenticationScenario_CreateLoginSuccessLoginFailure) {
    // Create User
    int userId = db->addUser("Alice Employee", "alice", "correct-password",
                              "alice@company.com", UserRole::EMPLOYEE);
    ASSERT_GT(userId, 0);

    // Login Success
    UserRecord rec = db->authenticate("alice", "correct-password");
    EXPECT_EQ(rec.id, userId);
    EXPECT_EQ(rec.role, UserRole::EMPLOYEE);

    // Login Failure -- wrong password
    EXPECT_THROW(db->authenticate("alice", "wrong-password"), AuthenticationException);

    // Login Failure -- unknown username
    EXPECT_THROW(db->authenticate("nobody", "whatever"), AuthenticationException);
}

TEST_F(TestDatabaseFixture, AuthenticationScenario_SeededAdminCanLogIn) {
    UserRecord rec = db->authenticate("admin", "admin123");
    EXPECT_EQ(rec.role, UserRole::ADMIN);
}

TEST_F(TestDatabaseFixture, AuthenticationScenario_UsernameLoginIsCaseInsensitive) {
    db->addUser("Alice Employee", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    // Uniqueness is case-insensitive; login lookup should be too, since
    // both are backed by the same COLLATE NOCASE column.
    UserRecord rec = db->authenticate("ALICE", "pw");
    EXPECT_EQ(rec.username, "alice");
}

TEST_F(TestDatabaseFixture, Registration_StoresPBKDF2HashNotPlaintext) {
    std::string plaintextPassword = "SecretPlaintextPassword!99";
    int userId = db->addUser("Bob Employee", "bob", plaintextPassword,
                             "bob@company.com", UserRole::EMPLOYEE);
    ASSERT_GT(userId, 0);

    UserRecord user = db->getUserById(userId);
    // Stored password field must be the encoded hash, never plaintext
    EXPECT_NE(user.password, plaintextPassword);
    EXPECT_EQ(user.password.rfind("pbkdf2_sha256$", 0), 0u);
    EXPECT_TRUE(PasswordHasher::verifyPassword(plaintextPassword, user.password));
}

TEST_F(TestDatabaseFixture, Authentication_HashSurvivesSQLiteReopen) {
    std::string plaintextPassword = "PersistentPassword2026";
    int userId = db->addUser("Carol Employee", "carol", plaintextPassword,
                             "carol@company.com", UserRole::EMPLOYEE);
    ASSERT_GT(userId, 0);

    // Reopen database connection
    db->close();
    db = std::make_unique<DatabaseManager>(dbPath);
    db->connect();

    // Authenticate against reopened database
    UserRecord rec = db->authenticate("carol", plaintextPassword);
    EXPECT_EQ(rec.id, userId);
    EXPECT_EQ(rec.username, "carol");

    // Wrong password must still fail after reopen
    EXPECT_THROW(db->authenticate("carol", "wrongpass"), AuthenticationException);
}

TEST_F(TestDatabaseFixture, Authentication_SeededAdminPasswordIsHashedInDatabase) {
    UserRecord admin = db->authenticate("admin", "admin123");
    EXPECT_EQ(admin.username, "admin");
    EXPECT_NE(admin.password, "admin123");
    EXPECT_EQ(admin.password.rfind("pbkdf2_sha256$", 0), 0u);
    EXPECT_TRUE(PasswordHasher::verifyPassword("admin123", admin.password));
}
