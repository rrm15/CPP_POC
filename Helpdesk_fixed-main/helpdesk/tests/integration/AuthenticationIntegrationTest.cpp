#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Common.h"

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
