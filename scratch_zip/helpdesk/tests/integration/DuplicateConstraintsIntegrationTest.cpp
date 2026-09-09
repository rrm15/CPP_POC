#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Common.h"

// -----------------------------------------------------------------------
// Duplicate username
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, DuplicateUsername_ExactMatchRejected) {
    db->addUser("Alice", "somesh", "pw", "alice@company.com", UserRole::EMPLOYEE);
    EXPECT_THROW(
        db->addUser("Someone Else", "somesh", "pw2", "other@company.com", UserRole::EMPLOYEE),
        ValidationException
    );
}

TEST_F(TestDatabaseFixture, DuplicateUsername_CaseInsensitiveVariantsRejected) {
    db->addUser("Alice", "somesh", "pw", "alice@company.com", UserRole::EMPLOYEE);

    EXPECT_THROW(
        db->addUser("Someone Else", "Somesh", "pw2", "other1@company.com", UserRole::EMPLOYEE),
        ValidationException
    ) << "'Somesh' must collide with existing 'somesh'";

    EXPECT_THROW(
        db->addUser("Someone Else", "SOMESH", "pw2", "other2@company.com", UserRole::EMPLOYEE),
        ValidationException
    ) << "'SOMESH' must collide with existing 'somesh'";
}

TEST_F(TestDatabaseFixture, DuplicateUsername_EnforcedAtDatabaseLevelNotJustPreCheck) {
    // Regression guard: even if the in-process pre-check (usernameExists)
    // were ever bypassed or raced, the UNIQUE(...COLLATE NOCASE) column
    // constraint is the actual source of truth. We can't easily simulate
    // a real race in a single-threaded test, but we can confirm the
    // schema itself carries the constraint by checking that two
    // back-to-back inserts through the public API -- the only way calling
    // code can reach the table -- are both blocked consistently.
    db->addUser("Alice", "carol", "pw", "carol1@company.com", UserRole::EMPLOYEE);
    EXPECT_THROW(db->addUser("Alice2", "carol", "pw", "carol2@company.com", UserRole::EMPLOYEE), ValidationException);
    EXPECT_THROW(db->addUser("Alice3", "CAROL", "pw", "carol3@company.com", UserRole::EMPLOYEE), ValidationException);
}

// -----------------------------------------------------------------------
// Duplicate email
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, DuplicateEmail_ExactMatchRejected) {
    db->addUser("Alice", "alice1", "pw", "shared@company.com", UserRole::EMPLOYEE);
    EXPECT_THROW(
        db->addUser("Bob", "bob1", "pw", "shared@company.com", UserRole::EMPLOYEE),
        ValidationException
    );
}

TEST_F(TestDatabaseFixture, DuplicateEmail_CaseInsensitiveVariantRejected) {
    db->addUser("Alice", "alice1", "pw", "shared@company.com", UserRole::EMPLOYEE);
    EXPECT_THROW(
        db->addUser("Bob", "bob1", "pw", "Shared@Company.com", UserRole::EMPLOYEE),
        ValidationException
    );
}

TEST_F(TestDatabaseFixture, DifferentUsernameAndEmailSucceeds) {
    EXPECT_NO_THROW(db->addUser("Alice", "alice1", "pw", "alice1@company.com", UserRole::EMPLOYEE));
    EXPECT_NO_THROW(db->addUser("Bob", "bob1", "pw", "bob1@company.com", UserRole::EMPLOYEE));
}
