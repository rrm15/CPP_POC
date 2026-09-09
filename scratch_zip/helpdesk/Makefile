# =============================================================================
# IT Helpdesk Ticket Management System -- Makefile
# -----------------------------------------------------------------------
# Thin wrapper around CMake/CTest/valgrind/cppcheck/gcovr. Each
# diagnostic target uses its own out-of-tree build directory so a normal
# `make` build is never polluted with coverage or sanitizer
# instrumentation.
#
#   build/            plain build (app + all three test suites)
#   build-coverage/   --coverage instrumented build, for `make coverage`
#   build-sanitize/   ASan+UBSan instrumented build, for `make sanitize`
#   build/logs/       valgrind / helgrind log output
# =============================================================================

BUILD_DIR       := build
COVERAGE_DIR    := build-coverage
SANITIZE_DIR    := build-sanitize
LOG_DIR         := $(BUILD_DIR)/logs
JOBS            := $(shell nproc 2>/dev/null || echo 4)

CMAKE           := cmake
CTEST           := ctest

.PHONY: all configure build unit integration system test \
        coverage valgrind helgrind valgrind-test helgrind-test \
        cppcheck sanitize run clean

# ---------------------------------------------------------------------------
# Plain build
# ---------------------------------------------------------------------------
all: build

configure:
	@mkdir -p $(BUILD_DIR)
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug

build: configure
	$(CMAKE) --build $(BUILD_DIR) -- -j$(JOBS)

run: build
	./$(BUILD_DIR)/helpdesk

# ---------------------------------------------------------------------------
# Test suites (via ctest labels; each also works as a standalone binary,
# e.g. ./build/helpdesk_unit_tests)
# ---------------------------------------------------------------------------
unit: build
	cd $(BUILD_DIR) && $(CTEST) -L unit --output-on-failure

integration: build
	cd $(BUILD_DIR) && $(CTEST) -L integration --output-on-failure

system: build
	cd $(BUILD_DIR) && $(CTEST) -L system --output-on-failure

test: build
	cd $(BUILD_DIR) && $(CTEST) --output-on-failure

# ---------------------------------------------------------------------------
# Coverage: separate instrumented build, run every suite, then report.
# Prefers gcovr (HTML + XML + console summary); falls back to a plain
# gcov summary if gcovr is not installed.
# ---------------------------------------------------------------------------
coverage:
	@mkdir -p $(COVERAGE_DIR)
	$(CMAKE) -S . -B $(COVERAGE_DIR) -DCMAKE_BUILD_TYPE=Debug -DHELPDESK_COVERAGE=ON
	$(CMAKE) --build $(COVERAGE_DIR) -- -j$(JOBS)
	cd $(COVERAGE_DIR) && $(CTEST) --output-on-failure
	@mkdir -p $(COVERAGE_DIR)/coverage-report
	@if command -v gcovr >/dev/null 2>&1; then \
		gcovr --root . --object-directory $(COVERAGE_DIR) \
			--filter 'src/.*' --filter 'include/.*' \
			--exclude-throw-branches \
			--print-summary \
			--html-details $(COVERAGE_DIR)/coverage-report/index.html \
			--xml $(COVERAGE_DIR)/coverage-report/coverage.xml; \
		echo "HTML report: $(COVERAGE_DIR)/coverage-report/index.html"; \
	else \
		echo "gcovr not found -- falling back to raw gcov output"; \
		gcov -r $$(find $(COVERAGE_DIR) -name '*.gcno'); \
	fi

# ---------------------------------------------------------------------------
# Lightweight runtime diagnostics: run the interactive application once
# under valgrind / helgrind. These are meant to be run by a developer
# attached to a real terminal (the app reads from stdin), so they are
# not part of `make test`.
# ---------------------------------------------------------------------------
valgrind: build
	valgrind \
		--leak-check=summary \
		--show-leak-kinds=definite,indirect \
		./$(BUILD_DIR)/helpdesk

helgrind: build
	valgrind \
		--tool=helgrind \
		--history-level=approx \
		./$(BUILD_DIR)/helpdesk

# ---------------------------------------------------------------------------
# Full validation targets: run entire test suites under valgrind/helgrind
# with strict settings and a non-zero exit code on any detected issue.
# ---------------------------------------------------------------------------
valgrind-test: build
	@mkdir -p $(LOG_DIR)
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--error-exitcode=1 \
		--log-file=$(LOG_DIR)/valgrind-unit.log \
		./$(BUILD_DIR)/helpdesk_unit_tests
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--error-exitcode=1 \
		--log-file=$(LOG_DIR)/valgrind-integration.log \
		./$(BUILD_DIR)/helpdesk_integration_tests
	valgrind \
		--leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--error-exitcode=1 \
		--log-file=$(LOG_DIR)/valgrind-system.log \
		./$(BUILD_DIR)/helpdesk_system_tests
	@echo "Valgrind logs written to $(LOG_DIR)/valgrind-{unit,integration,system}.log"

helgrind-test: build
	@mkdir -p $(LOG_DIR)
	valgrind \
		--tool=helgrind \
		--history-level=full \
		--error-exitcode=1 \
		--log-file=$(LOG_DIR)/helgrind-system.log \
		./$(BUILD_DIR)/helpdesk_system_tests
	@echo "Helgrind log written to $(LOG_DIR)/helgrind-system.log"
	@echo "NOTE: this application is single-threaded, so a clean helgrind"
	@echo "run is expected; the target exists so any future concurrency"
	@echo "work is covered from day one."

# ---------------------------------------------------------------------------
# Static analysis
# ---------------------------------------------------------------------------
cppcheck:
	@mkdir -p $(LOG_DIR)
	cppcheck \
		--enable=all \
		--inconclusive \
		--std=c++17 \
		--suppress=missingIncludeSystem \
		-I include \
		src app \
		2> $(LOG_DIR)/cppcheck.log; \
	cat $(LOG_DIR)/cppcheck.log; \
	echo "Full report: $(LOG_DIR)/cppcheck.log"

# ---------------------------------------------------------------------------
# Sanitizers: separate instrumented build; ASan/UBSan activate at
# runtime automatically, so running the binaries is the whole test.
#
# Before attempting the real (potentially expensive) instrumented build,
# probe whether the compiler CMake/the shell will actually resolve
# (respecting a caller-supplied CXX=... override, same as CMake would)
# has a working AddressSanitizer runtime alongside it. Some toolchain
# installations (e.g. Red Hat Software-Collections-style alternate
# gcc-toolset paths) accept -fsanitize=address at compile time but have
# no libasan installed, which fails only at link time with an opaque
# "cannot find -lasan" error. Catching that here, with a name-the-culprit
# message, is much more actionable than letting the real build fail deep
# inside CMake/CTest output -- and we deliberately fail *non-zero* rather
# than silently skipping, so CI/a human notices that sanitizer coverage
# did not actually run.
# ---------------------------------------------------------------------------
SANITIZE_CXX := $(if $(CXX),$(CXX),g++)
SANITIZE_PROBE_DIR := $(SANITIZE_DIR)/.asan-probe

sanitize:
	@mkdir -p $(SANITIZE_PROBE_DIR)
	@echo 'int main(){return 0;}' > $(SANITIZE_PROBE_DIR)/probe.cpp
	@if $(SANITIZE_CXX) -fsanitize=address,undefined -o $(SANITIZE_PROBE_DIR)/probe \
			$(SANITIZE_PROBE_DIR)/probe.cpp > $(SANITIZE_PROBE_DIR)/probe.log 2>&1; then \
		echo "AddressSanitizer/UBSan runtime probe OK ($(SANITIZE_CXX))."; \
	else \
		echo ""; \
		echo "=========================================================="; \
		echo " make sanitize: ABORTED -- ASan/UBSan runtime unavailable"; \
		echo "=========================================================="; \
		echo "Compiler probed : $(SANITIZE_CXX)"; \
		echo "Resolved path   : $$(command -v $(SANITIZE_CXX) 2>/dev/null || echo 'not found on PATH')"; \
		echo ""; \
		echo "A trivial -fsanitize=address,undefined program failed to"; \
		echo "compile+link with this compiler. This almost always means"; \
		echo "the compiler frontend accepts the sanitizer flags but the"; \
		echo "matching runtime library (libasan / libubsan) is not"; \
		echo "installed alongside this specific toolchain -- a known"; \
		echo "issue with some alternate-toolchain installs (e.g. Red Hat"; \
		echo "Software Collections' gcc-toolset-N paths)."; \
		echo ""; \
		echo "Probe compiler/linker output:"; \
		sed 's/^/    /' $(SANITIZE_PROBE_DIR)/probe.log; \
		echo ""; \
		echo "Fix options:"; \
		echo "  (a) Install the sanitizer runtime package that matches"; \
		echo "      this toolchain (e.g. the libasan/libubsan devel"; \
		echo "      package for $(SANITIZE_CXX)'s package family)."; \
		echo "  (b) Rebuild using a system-default compiler that already"; \
		echo "      ships libasan, e.g.:"; \
		echo "        make sanitize CXX=/usr/bin/g++"; \
		echo "      or pass it straight to CMake:"; \
		echo "        cmake -S . -B $(SANITIZE_DIR) -DHELPDESK_SANITIZE=ON \\"; \
		echo "              -DCMAKE_CXX_COMPILER=/usr/bin/g++"; \
		echo "=========================================================="; \
		exit 1; \
	fi
	$(CMAKE) -S . -B $(SANITIZE_DIR) -DCMAKE_BUILD_TYPE=Debug -DHELPDESK_SANITIZE=ON \
		$(if $(CXX),-DCMAKE_CXX_COMPILER=$(CXX),)
	$(CMAKE) --build $(SANITIZE_DIR) -- -j$(JOBS)
	cd $(SANITIZE_DIR) && $(CTEST) --output-on-failure

# ---------------------------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR) $(COVERAGE_DIR) $(SANITIZE_DIR) helpdesk.db
	find . -maxdepth 1 -name 'test_*.db' -delete
