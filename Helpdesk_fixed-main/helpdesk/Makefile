# ============================================================================
# IT Helpdesk Ticket Management System - Canonical Makefile
# ============================================================================

BUILD_ROOT := build
DEBUG_DIR := $(BUILD_ROOT)/debug
COVERAGE_DIR := $(BUILD_ROOT)/coverage
SANITIZE_DIR := $(BUILD_ROOT)/sanitize
LOG_DIR := $(BUILD_ROOT)/logs
REPORT_DIR := $(BUILD_ROOT)/reports

CMAKE ?= cmake
CTEST ?= ctest
PYTHON ?= python3

.PHONY: all build run test test-count coverage coverage-summary valgrind valgrind-test helgrind helgrind-test cppcheck sanitize clean help

# Default target: configure & build Debug tree under build/debug/
all: $(DEBUG_DIR)/CMakeCache.txt
	@$(CMAKE) --build $(DEBUG_DIR) -- -j"$$(nproc 2>/dev/null || echo 4)"

$(DEBUG_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(DEBUG_DIR)
	@$(CMAKE) -S . -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug

build: all

run: all
	@./$(DEBUG_DIR)/helpdesk

test: all
	@$(CTEST) --test-dir $(DEBUG_DIR) --output-on-failure

test-count: $(DEBUG_DIR)/CMakeCache.txt
	@$(CMAKE) --build $(DEBUG_DIR) -- -j"$$(nproc 2>/dev/null || echo 4)" > /dev/null 2>&1
	@COUNT=$$($(CTEST) --test-dir $(DEBUG_DIR) -N | grep -oP '\d+(?= tests)' | head -1); \
	if [ -z "$$COUNT" ]; then \
		COUNT=$$($(CTEST) --test-dir $(DEBUG_DIR) -N | grep -i 'Total Tests:' | awk '{print $$3}'); \
	fi; \
	echo "Authoritative CTest count: $$COUNT"

# Coverage Target with stage markers
$(COVERAGE_DIR)/CMakeCache.txt: CMakeLists.txt
	@echo "[1/4] Configure"
	@mkdir -p $(COVERAGE_DIR)
	@$(CMAKE) -S . -B $(COVERAGE_DIR) -DCMAKE_BUILD_TYPE=Debug -DHELPDESK_COVERAGE=ON > /dev/null

coverage:
	@rm -rf $(COVERAGE_DIR)
	@$(MAKE) $(COVERAGE_DIR)/CMakeCache.txt
	@echo "[2/4] Build"
	@$(CMAKE) --build $(COVERAGE_DIR) -- -j"$$(nproc 2>/dev/null || echo 4)" > /dev/null
	@echo "[3/4] Run tests"
	@$(CTEST) --test-dir $(COVERAGE_DIR) --output-on-failure
	@echo "[4/4] Generate report"
	@$(PYTHON) scripts/coverage_report.py $(COVERAGE_DIR)

coverage-summary:
	@$(PYTHON) scripts/coverage_report.py $(COVERAGE_DIR)

# Diagnostic targets - logs output to build/logs/<tool>/
valgrind: all
	@mkdir -p $(LOG_DIR)/valgrind
	valgrind --leak-check=full --show-leak-kinds=all --log-file=$(LOG_DIR)/valgrind/valgrind.log ./$(DEBUG_DIR)/helpdesk
	@echo "Valgrind log saved to $(LOG_DIR)/valgrind/valgrind.log"

valgrind-test: all
	@mkdir -p $(LOG_DIR)/valgrind
	valgrind --leak-check=full --log-file=$(LOG_DIR)/valgrind/unit.log ./$(DEBUG_DIR)/helpdesk_unit_tests
	valgrind --leak-check=full --log-file=$(LOG_DIR)/valgrind/integration.log ./$(DEBUG_DIR)/helpdesk_integration_tests
	valgrind --leak-check=full --log-file=$(LOG_DIR)/valgrind/system.log ./$(DEBUG_DIR)/helpdesk_system_tests
	@echo "Valgrind test logs saved to $(LOG_DIR)/valgrind/"

helgrind: all
	@mkdir -p $(LOG_DIR)/helgrind
	valgrind --tool=helgrind --log-file=$(LOG_DIR)/helgrind/helgrind.log ./$(DEBUG_DIR)/helpdesk
	@echo "Helgrind log saved to $(LOG_DIR)/helgrind/helgrind.log"

helgrind-test: all
	@mkdir -p $(LOG_DIR)/helgrind
	valgrind --tool=helgrind --log-file=$(LOG_DIR)/helgrind/system.log ./$(DEBUG_DIR)/helpdesk_system_tests
	@echo "Helgrind test log saved to $(LOG_DIR)/helgrind/system.log"

cppcheck:
	@mkdir -p $(LOG_DIR)/cppcheck
	cppcheck --enable=all --suppress=missingIncludeSystem --quiet src/ include/ 2>&1 | tee $(LOG_DIR)/cppcheck/cppcheck.log
	@echo "Cppcheck log saved to $(LOG_DIR)/cppcheck/cppcheck.log"

# Sanitizer target - uses build/sanitize/
$(SANITIZE_DIR)/CMakeCache.txt: CMakeLists.txt
	@mkdir -p $(SANITIZE_DIR)
	@$(CMAKE) -S . -B $(SANITIZE_DIR) -DCMAKE_BUILD_TYPE=Debug -DHELPDESK_SANITIZE=ON

sanitize: $(SANITIZE_DIR)/CMakeCache.txt
	@$(CMAKE) --build $(SANITIZE_DIR) -- -j"$$(nproc 2>/dev/null || echo 4)"
	@$(CTEST) --test-dir $(SANITIZE_DIR) --output-on-failure

reset-db:
	rm -f *.db test_*.db helpdesk.db

# Clean target - removes the build/ directory only
clean:
	rm -rf $(BUILD_ROOT) *.gcov *.log

help:
	@echo "Available Makefile targets:"
	@echo "  make (all)           - Configure and build Debug targets in $(DEBUG_DIR)"
	@echo "  make run             - Launch build/debug/helpdesk"
	@echo "  make test            - Execute all CTest test suites in $(DEBUG_DIR)"
	@echo "  make test-count      - Dynamically count and display total registered CTest cases"
	@echo "  make coverage        - Configure, build with gcov, run tests, and generate coverage report"
	@echo "  make coverage-summary- Render coverage report from existing build/coverage artifacts"
	@echo "  make valgrind        - Run helpdesk app under Valgrind Memcheck (logged to build/logs/valgrind/)"
	@echo "  make valgrind-test   - Run test suites under Valgrind Memcheck (logged to build/logs/valgrind/)"
	@echo "  make helgrind        - Run helpdesk app under Valgrind Helgrind (logged to build/logs/helgrind/)"
	@echo "  make helgrind-test   - Run system test suite under Valgrind Helgrind (logged to build/logs/helgrind/)"
	@echo "  make cppcheck        - Run static analysis and log to build/logs/cppcheck/cppcheck.log"
	@echo "  make sanitize        - Build with ASan+UBSan in build/sanitize/ and run test suites"
	@echo "  make clean           - Remove the entire build/ directory"
	@echo "  make help            - Display this target summary"
