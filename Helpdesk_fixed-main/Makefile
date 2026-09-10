.PHONY: all clean build release debug test coverage coverage-report coverage-clean help install

# Build directory
BUILD_DIR := build
RELEASE_BUILD := $(BUILD_DIR)/release
DEBUG_BUILD := $(BUILD_DIR)/debug
COVERAGE_BUILD := $(BUILD_DIR)/coverage

# Default target
all: build

help:
	@echo "Helpdesk Build System"
	@echo "===================="
	@echo ""
	@echo "Available targets:"
	@echo "  make build           - Build project in debug mode (default)"
	@echo "  make release         - Build optimized release version"
	@echo "  make debug           - Build debug version with symbols"
	@echo "  make test            - Build and run unit tests"
	@echo "  make coverage        - Build with code coverage instrumentation"
	@echo "  make coverage-report - Generate HTML coverage report (requires lcov)"
	@echo "  make coverage-clean  - Clean coverage data"
	@echo "  make clean           - Remove all build artifacts"
	@echo "  make install         - Install built executable"
	@echo "  make help            - Display this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make release         # Build optimized version"
	@echo "  make coverage        # Build with gcov coverage"
	@echo "  make test            # Run tests"
	@echo ""

# Build debug version
build: debug

debug: $(DEBUG_BUILD)
	cd $(DEBUG_BUILD) && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=OFF .. && $(MAKE)
	@echo "✓ Debug build complete. Binary: $(DEBUG_BUILD)/helpdesk"

# Build release version
release: $(RELEASE_BUILD)
	cd $(RELEASE_BUILD) && cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_COVERAGE=OFF .. && $(MAKE)
	@echo "✓ Release build complete. Binary: $(RELEASE_BUILD)/helpdesk"

# Build with coverage instrumentation
coverage: $(COVERAGE_BUILD)
	cd $(COVERAGE_BUILD) && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON .. && $(MAKE)
	@echo "✓ Coverage-instrumented build complete"
	@echo "  Run tests with: cd $(COVERAGE_BUILD) && ctest"
	@echo "  Generate report with: make coverage-report"

# Run tests
test: debug
	cd $(DEBUG_BUILD) && ctest --output-on-failure
	@echo "✓ All tests passed!"

# Generate coverage report (requires lcov and genhtml)
coverage-report: coverage
	@command -v lcov >/dev/null 2>&1 || { echo "lcov not installed. Install with: sudo apt-get install lcov"; exit 1; }
	@echo "Running tests to collect coverage data..."
	cd $(COVERAGE_BUILD) && ctest --output-on-failure
	@echo "Collecting coverage information..."
	lcov --directory $(COVERAGE_BUILD) --capture --output-file $(COVERAGE_BUILD)/coverage.info
	lcov --remove $(COVERAGE_BUILD)/coverage.info '/usr/*' --output-file $(COVERAGE_BUILD)/coverage.info
	@echo "Generating HTML report..."
	genhtml $(COVERAGE_BUILD)/coverage.info --output-directory $(COVERAGE_BUILD)/coverage_report
	@echo "✓ Coverage report generated: $(COVERAGE_BUILD)/coverage_report/index.html"

# Clean coverage data
coverage-clean:
	@echo "Cleaning coverage data..."
	find $(COVERAGE_BUILD) -name "*.gcda" -delete
	find $(COVERAGE_BUILD) -name "*.gcov" -delete
	rm -f $(COVERAGE_BUILD)/coverage.info
	rm -rf $(COVERAGE_BUILD)/coverage_report
	@echo "✓ Coverage data cleaned"

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(RELEASE_BUILD): $(BUILD_DIR)
	mkdir -p $(RELEASE_BUILD)

$(DEBUG_BUILD): $(BUILD_DIR)
	mkdir -p $(DEBUG_BUILD)

$(COVERAGE_BUILD): $(BUILD_DIR)
	mkdir -p $(COVERAGE_BUILD)

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@echo "✓ Build artifacts removed"

# Install executable
install: release
	@command -v install >/dev/null 2>&1 || { echo "install command not found"; exit 1; }
	install -m 755 $(RELEASE_BUILD)/helpdesk /usr/local/bin/helpdesk
	@echo "✓ Installed to /usr/local/bin/helpdesk"

# Info target
info:
	@echo "Build Configuration:"
	@echo "  Build Directory: $(BUILD_DIR)"
	@echo "  Debug Build: $(DEBUG_BUILD)"
	@echo "  Release Build: $(RELEASE_BUILD)"
	@echo "  Coverage Build: $(COVERAGE_BUILD)"
	@echo ""
	@echo "System Information:"
	@uname -a
	@echo ""
	@command -v cmake >/dev/null 2>&1 && cmake --version || echo "CMake: NOT FOUND"
	@command -v gcc >/dev/null 2>&1 && gcc --version | head -1 || echo "GCC: NOT FOUND"
	@command -v sqlite3 >/dev/null 2>&1 && sqlite3 --version || echo "SQLite3: NOT FOUND"

.SILENT: info help
