cmake_minimum_required(VERSION 3.16)
project(ITHelpdeskTicketSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# ---------------------------------------------------------------------------
# Optional instrumentation, off by default so a plain `cmake --build .`
# produces a normal, fast binary. Toggle with -DHELPDESK_COVERAGE=ON or
# -DHELPDESK_SANITIZE=ON (mutually exclusive in practice -- coverage and
# ASan/UBSan can be combined, but the Makefile drives them as separate
# build directories to keep results easy to interpret).
# ---------------------------------------------------------------------------
option(HELPDESK_COVERAGE "Build with --coverage instrumentation (gcov/gcovr)" OFF)
option(HELPDESK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(HELPDESK_BUILD_TESTS "Build the GoogleTest-based test suites" ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

# ---------------------------------------------------------------------------
# helpdesk_core: all domain/service/repository logic, with NO main().
# Both the console executable and every test target link against this
# single static library, so production and test code always exercise
# identical compiled logic (no "test-only" reimplementations).
# ---------------------------------------------------------------------------
add_library(helpdesk_core STATIC
    src/Ticket.cpp
    src/User.cpp
    src/DatabaseManager.cpp
    src/PasswordHasher.cpp
    src/PasswordInput.cpp
    src/Validation.cpp
)
target_include_directories(helpdesk_core PUBLIC include ${SQLite3_INCLUDE_DIRS})
target_link_libraries(helpdesk_core PUBLIC ${SQLite3_LIBRARIES} OpenSSL::Crypto)
target_compile_options(helpdesk_core PRIVATE -Wall -Wextra)

if(HELPDESK_COVERAGE)
  target_compile_options(helpdesk_core PUBLIC -O0 -g --coverage)
  target_link_options(helpdesk_core PUBLIC --coverage)
endif()

if(HELPDESK_SANITIZE)
  target_compile_options(helpdesk_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -g -O0)
  target_link_options(helpdesk_core PUBLIC -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# helpdesk: the console application
# ---------------------------------------------------------------------------
add_executable(helpdesk app/main.cpp)
target_link_libraries(helpdesk PRIVATE helpdesk_core)
target_compile_options(helpdesk PRIVATE -Wall -Wextra)

if(HELPDESK_COVERAGE)
  target_compile_options(helpdesk PRIVATE -O0 -g --coverage)
  target_link_options(helpdesk PRIVATE --coverage)
endif()
if(HELPDESK_SANITIZE)
  target_compile_options(helpdesk PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g -O0)
  target_link_options(helpdesk PRIVATE -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Tests. GoogleTest packaging is genuinely inconsistent across distros:
# some (e.g. Ubuntu's libgtest-dev/libgmock-dev) ship a CMake package-
# config file and work with `find_package(GTest CONFIG)`; others
# (frequently EL8-family systems -- RHEL/CentOS/Rocky/Alma 8) ship only
# headers + static libs with NO CMake config integration at all, so
# CONFIG mode fails outright at configure time. Try both packaging
# styles before ever falling back to FetchContent, which stays a last
# resort per project policy (slow, requires network, and hides what's
# actually installed on the box).
# ---------------------------------------------------------------------------
if(HELPDESK_BUILD_TESTS)
  enable_testing()

  # 1) Preferred: distro package that ships GTestConfig.cmake /
  #    gtest-config.cmake (e.g. Ubuntu libgtest-dev + libgmock-dev).
  find_package(GTest CONFIG QUIET)

  if(GTest_FOUND)
    message(STATUS "GTest: found via CONFIG mode (package-config file present).")
  else()
    # 2) Fallback: CMake's bundled FindGTest module, which locates GTest
    #    from plain headers + .a/.so files without needing any
    #    distro-provided CMake package-config -- this is what covers
    #    EL8-family systems that only ship the raw library + headers.
    find_package(GTest MODULE QUIET)
    if(GTest_FOUND)
      message(STATUS "GTest: found via MODULE (FindGTest) mode -- no package-config file on this system.")
    endif()
  endif()

  # GMock target names differ (and existence differs) between the two
  # modes, and this project's test sources don't currently include
  # gmock at all -- so only require/link it if some test file actually
  # uses it, rather than hard-failing configure over an unused
  # dependency.
  file(GLOB_RECURSE HELPDESK_TEST_SOURCES tests/*.cpp)
  set(HELPDESK_TESTS_USE_GMOCK FALSE)
  foreach(_src ${HELPDESK_TEST_SOURCES})
    file(STRINGS "${_src}" _gmock_hit REGEX "gmock")
    if(_gmock_hit)
      set(HELPDESK_TESTS_USE_GMOCK TRUE)
    endif()
  endforeach()

  if(NOT GTest_FOUND)
    # 3) Last resort: fetch GoogleTest sources and build it as part of
    #    this project. Only reached if neither CONFIG nor MODULE
    #    discovery found a local installation of either packaging style.
    message(WARNING
      "GTest: no local installation found via CONFIG or MODULE lookup "
      "(checked both distro packaging styles). Falling back to "
      "FetchContent -- this requires network access and will be slower "
      "than using a local package. Install a GTest development package "
      "for this distro (e.g. libgtest-dev/libgmock-dev on Debian/Ubuntu, "
      "or gtest-devel/gmock-devel on Fedora/EL) to avoid this.")
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    # Targets built via add_subdirectory/FetchContent come from
    # googletest's own CMakeLists.txt, which names them `gtest`/`gmock`
    # (no GTest:: namespace) -- unlike either find_package() mode above.
    set(HELPDESK_GTEST_LIB gtest)
    set(HELPDESK_GMOCK_LIB gmock)
  elseif(TARGET GTest::gtest)
    # CONFIG mode target names.
    set(HELPDESK_GTEST_LIB GTest::gtest)
    set(HELPDESK_GMOCK_LIB $<$<BOOL:${HELPDESK_TESTS_USE_GMOCK}>:GTest::gmock>)
  else()
    # MODULE mode exposes GTest::GTest/GTest::Main and typically no
    # gmock target at all.
    set(HELPDESK_GTEST_LIB GTest::GTest)
    if(HELPDESK_TESTS_USE_GMOCK)
      message(FATAL_ERROR
        "GTest was found via MODULE mode, which does not provide a "
        "GMock target, but test sources under tests/ reference gmock. "
        "Install a package that provides GMock (e.g. libgmock-dev) or "
        "a GTest packaging with CONFIG-mode support.")
    endif()
    set(HELPDESK_GMOCK_LIB "")
  endif()

  include(GoogleTest)

  function(helpdesk_add_test_suite suite_name)
    # suite_name: unit | integration | system
    file(GLOB ${suite_name}_SOURCES tests/${suite_name}/*.cpp)
    if(${suite_name}_SOURCES)
      add_executable(helpdesk_${suite_name}_tests
          tests/test_main.cpp
          ${${suite_name}_SOURCES}
      )
      target_include_directories(helpdesk_${suite_name}_tests PRIVATE tests/fixtures)
      target_link_libraries(helpdesk_${suite_name}_tests PRIVATE
          helpdesk_core
          ${HELPDESK_GTEST_LIB}
      )
      if(HELPDESK_GMOCK_LIB)
        target_link_libraries(helpdesk_${suite_name}_tests PRIVATE ${HELPDESK_GMOCK_LIB})
      endif()
      target_compile_options(helpdesk_${suite_name}_tests PRIVATE -Wall -Wextra)

      if(HELPDESK_COVERAGE)
        target_compile_options(helpdesk_${suite_name}_tests PRIVATE -O0 -g --coverage)
        target_link_options(helpdesk_${suite_name}_tests PRIVATE --coverage)
      endif()
      if(HELPDESK_SANITIZE)
        target_compile_options(helpdesk_${suite_name}_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g -O0)
        target_link_options(helpdesk_${suite_name}_tests PRIVATE -fsanitize=address,undefined)
      endif()

      # Run each test binary from the build directory so the throwaway
      # per-test SQLite files it creates land in one predictable,
      # gitignored place instead of scattering into the source tree.
      gtest_discover_tests(helpdesk_${suite_name}_tests
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          PROPERTIES LABELS ${suite_name}
      )
    endif()
  endfunction()

  helpdesk_add_test_suite(unit)
  helpdesk_add_test_suite(integration)
  helpdesk_add_test_suite(system)
endif()
