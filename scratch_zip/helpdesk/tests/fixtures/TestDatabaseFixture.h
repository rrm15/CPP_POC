#ifndef TEST_DATABASE_FIXTURE_H
#define TEST_DATABASE_FIXTURE_H

#include <gtest/gtest.h>
#include <cstdio>
#include <string>
#include <memory>
#include "DatabaseManager.h"

// -----------------------------------------------------------------------
// TestDatabaseFixture
// -----------------------------------------------------------------------
// A gtest fixture that creates a brand-new, uniquely-named SQLite file
// per test (never the production `helpdesk.db`), initializes the schema
// against it, and deletes the file again on teardown. Used by every
// integration test and by the repository-style tests under unit/ that
// need real persistence.
//
// Uniqueness per test is derived from the current test's name so
// parallel/parametrized tests never collide on the same file.
// -----------------------------------------------------------------------
class TestDatabaseFixture : public ::testing::Test {
protected:
    std::string dbPath;
    std::unique_ptr<DatabaseManager> db;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string raw = std::string("test_") + info->test_suite_name() + "_" + info->name() + ".db";
        // Parametrized/typed test names can contain '/' (e.g.
        // "AllTicketTypes/TicketTypePersistenceTest/0"), which would
        // otherwise be interpreted as a directory separator and produce
        // an invalid path SQLite can't open. Flatten it to a safe,
        // filesystem-legal filename.
        for (char& c : raw) {
            if (c == '/' || c == '\\') c = '_';
        }
        dbPath = raw;
        std::remove(dbPath.c_str()); // in case a previous run crashed before cleanup

        db = std::make_unique<DatabaseManager>(dbPath);
        db->connect();
        db->initializeSchema();
    }

    void TearDown() override {
        db->close();
        db.reset();
        std::remove(dbPath.c_str());
    }
};

#endif // TEST_DATABASE_FIXTURE_H
