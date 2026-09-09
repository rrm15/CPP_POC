// Custom test runner entry point. All three test suites (unit,
// integration, system) link against this single main() rather than
// GTest::gtest_main, so global setup/teardown (if ever needed -- e.g.
// silencing stdout noise from the console-menu code under test) has one
// obvious place to live.
#include <gtest/gtest.h>
#include <sqlite3.h>

int main(int argc, char** argv) {
    // Must precede any sqlite3 API call made by any fixture/test in this
    // binary (every TestDatabaseFixture-based test constructs its own
    // DatabaseManager on this same single thread). See the identical call
    // and rationale in app/main.cpp -- this eliminates the SQLite/Helgrind
    // false-positive "recursive lock" reports and speeds up helgrind runs
    // by removing SQLite's internal mutex bookkeeping entirely, which is
    // safe because this test process never shares a connection across
    // threads.
    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
