cmake_minimum_required(VERSION 3.10)
project(Helpdesk)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable coverage testing
option(ENABLE_COVERAGE "Enable code coverage" OFF)

if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
    message(STATUS "Code coverage enabled with gcov")
endif()

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/User.cpp
    src/Ticket.cpp
    src/DatabaseManager.cpp
    src/Validation.cpp
    src/PasswordInput.cpp
    src/InputUtil.cpp
    src/DateUtil.cpp
)

# Create main executable
add_executable(helpdesk app/main.cpp ${SOURCES})
target_link_libraries(helpdesk PRIVATE SQLite3::SQLite3)

# Enable testing
enable_testing()

# Add test executable
add_executable(helpdesk_tests
    ${SOURCES}
    tests/unit/ValidationTest.cpp
    tests/unit/PasswordInputTest.cpp
    tests/unit/TicketTest.cpp
    tests/integration/TicketPersistenceIntegrationTest.cpp
    tests/integration/AuthenticationIntegrationTest.cpp
    tests/integration/DuplicateConstraintsIntegrationTest.cpp
    tests/integration/ResolutionWorkflowIntegrationTest.cpp
    tests/system/ConsoleMenuWorkflowTest.cpp
    tests/system/SystemWorkflowTest.cpp
    tests/system/InvalidInputResilienceTest.cpp
    tests/test_main.cpp
)
target_link_libraries(helpdesk_tests PRIVATE SQLite3::SQLite3)

# Register tests
add_test(NAME ValidationTests COMMAND helpdesk_tests)
add_test(NAME IntegrationTests COMMAND helpdesk_tests)
add_test(NAME SystemTests COMMAND helpdesk_tests)

# Print configuration
message(STATUS "Helpdesk Project Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Code Coverage: ${ENABLE_COVERAGE}")
message(STATUS "  SQLite3 Found: TRUE")
message(STATUS "  Build Directory: ${CMAKE_BINARY_DIR}")

# Concurrent Write Safeguards
# The DatabaseManager uses the following strategies:
# 1. SQLite FULLMUTEX mode for thread-safe database access
# 2. Conditional updates (WHERE clauses) to prevent race conditions
# 3. Transaction support for atomic operations
# 4. Prepared statements to prevent SQL injection
# See src/DatabaseManager.cpp for implementation details
