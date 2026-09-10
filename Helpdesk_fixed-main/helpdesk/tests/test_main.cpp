// Custom test runner entry point. All three test suites (unit,
// integration, system) link against this single main() rather than
// GTest::gtest_main, so global setup/teardown (if ever needed) has one
// obvious place to live.
#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

