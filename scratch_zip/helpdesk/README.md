# IT Helpdesk Ticket Management System

A console-based IT Helpdesk Ticket Management System in modern C++17,
backed by a real SQLite3 database, with a GoogleTest-based three-layer
test suite (unit / integration / system) and a full diagnostics
toolchain (Valgrind, Helgrind, Cppcheck, ASan/UBSan, gcovr coverage).

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Features](#2-features)
3. [Technology Stack](#3-technology-stack)
4. [Architecture](#4-architecture)
5. [Database Schema](#5-database-schema)
6. [Build Instructions](#6-build-instructions)
7. [Application Usage](#7-application-usage)
8. [Ticket Types](#8-ticket-types)
9. [Validation Rules](#9-validation-rules)
10. [Testing](#10-testing)
11. [Diagnostics (Valgrind / Helgrind)](#11-diagnostics-valgrind--helgrind)
12. [Static Analysis (Cppcheck)](#12-static-analysis-cppcheck)
13. [Sanitizers](#13-sanitizers)
14. [Coverage](#14-coverage)
15. [Security Considerations](#15-security-considerations)
16. [File-by-File Change Summary](#16-file-by-file-change-summary)
17. [Validation Checklist](#17-validation-checklist)
18. [Real-Machine Bugfix Pass](#18-real-machine-bugfix-pass)
19. [Future Enhancements](#19-future-enhancements)

---

## 1. Project Overview

The system lets employees raise IT support tickets, administrators
assign them to engineers, and engineers work them through to
resolution — with every action persisted in SQLite so nothing is lost
between sessions. This revision hardens the original prototype into a
more production-shaped codebase: masked password entry, ticket
categorization, a corrected resolved-ticket workflow, real input
validation with database-level enforcement, and a proper automated
test/diagnostics pipeline.

## 2. Features

- Role-based console menus: **Employee**, **Engineer**, **Admin**
  (polymorphic dispatch through an abstract `User` base class)
- Ticket lifecycle: `OPEN → ASSIGNED → IN_PROGRESS → RESOLVED → CLOSED`
- **Ticket type classification** (8 categories, see [§8](#8-ticket-types))
- **Masked password input** (`*` echoed, real terminal raw-mode handling)
- **Resolved-ticket workflow separation** ("Assigned Tickets" no longer
  shows resolved/closed tickets; a dedicated "View Resolved Tickets"
  screen was added for Engineer and Admin)
- **Username & email validation**, enforced both in the console layer
  and at the database layer (`UNIQUE ... COLLATE NOCASE`)
- SQLite persistence via parameterized queries (`sqlite3_prepare_v2` +
  `sqlite3_bind_*` everywhere — no string-concatenated SQL)
- Reporting: ticket counts by status/priority, average feedback rating

## 3. Technology Stack

| Component        | Choice                                   |
|-------------------|-------------------------------------------|
| Language           | C++17                                     |
| Database            | SQLite3 (C API, prepared statements)      |
| Build system         | CMake ≥ 3.16 (+ a Makefile wrapper)      |
| Testing              | GoogleTest / GoogleMock (via `find_package(GTest CONFIG REQUIRED)`) |
| Memory/thread diagnostics | Valgrind (Memcheck) / Helgrind      |
| Static analysis       | Cppcheck                                |
| Sanitizers             | AddressSanitizer + UndefinedBehaviorSanitizer |
| Coverage                | gcov + gcovr (HTML/XML/console)       |

All of the above were installed and exercised for real in the
environment this project was built in (Ubuntu 24.04 / GCC 13); none of
the commands or numbers in this document are hypothetical.

## 4. Architecture

```
helpdesk/
├── include/                  Headers (declarations only)
│   ├── Common.h                Enums (UserRole, TicketStatus, TicketPriority,
│   │                           TicketType) + string conversions + the
│   │                           custom exception hierarchy
│   ├── Validation.h             Username/email format validation (pure,
│   │                            no DB dependency — the "validation layer")
│   ├── PasswordInput.h          Masked password entry (termios), with a
│   │                            testable keystroke core separated from
│   │                            the terminal I/O wrapper
│   ├── DateUtil.h                Timestamp helper
│   ├── InputUtil.h                Console input helpers (readInt, ticket
│   │                              type/priority menus, etc.)
│   ├── Ticket.h                    Ticket entity (encapsulated; behavior
│   │                               methods: assignTo/updateStatus/resolve/
│   │                               addFeedback)
│   ├── User.h                      Abstract User base + Employee/Engineer/
│   │                               Admin ("service" layer — each role's
│   │                               console workflows)
│   └── DatabaseManager.h            SQLite data-access layer ("repository")
├── src/                       Implementations of everything above
│   (compiled into the helpdesk_core static library — no main())
├── app/
│   └── main.cpp                The only file with main(); login/registration
│                                flow, wires DatabaseManager to a User
├── tests/
│   ├── fixtures/
│   │   └── TestDatabaseFixture.h   Per-test throwaway SQLite file (never
│   │                               the production helpdesk.db)
│   ├── unit/                  Ticket, Validation, PasswordInput, and
│   │                          repository (DatabaseManager) unit tests
│   ├── integration/           Multi-step scenarios against a real
│   │                          temporary SQLite database
│   ├── system/                Full end-to-end workflows, including tests
│   │                          that drive the actual console menus
│   │                          (Employee/Engineer/Admin::showMenu) via
│   │                          redirected stdin/stdout
│   └── test_main.cpp          Shared GoogleTest entry point
├── CMakeLists.txt             helpdesk_core lib + helpdesk exe + 3 test
│                               binaries, GTest via find_package, coverage/
│                               sanitizer build options
├── Makefile                   make unit/integration/system/test/coverage/
│                               valgrind/helgrind/valgrind-test/helgrind-test/
│                               cppcheck/sanitize
└── README.md
```

### Layers

- **Models**: `Ticket`, plus the `UserRecord` DTO used to move raw rows
  out of `DatabaseManager` before the caller reconstructs the correct
  `User` subclass.
- **Services**: `Employee`, `Engineer`, `Admin` — each owns its own
  console workflows and calls into `DatabaseManager` and `Validation`.
- **Repository / Persistence**: `DatabaseManager` — the *only* class
  that touches `sqlite3*`/`sqlite3_stmt*`. All filtering (e.g.
  active-vs-resolved tickets) happens in SQL `WHERE` clauses here, not
  by fetching everything and filtering in C++, so there is one
  authoritative definition of "active" vs "resolved."
- **Validation layer**: `Validation.h/.cpp` — pure, dependency-free
  functions reused by both the console layer (fast, friendly UX) and
  `DatabaseManager` (defense-in-depth; never relies solely on UI
  validation).
- **Testing layer**: `helpdesk_core` static library linked identically
  by the app and by all three test binaries, so tests exercise the
  real production code, never a reimplementation.

### Design decisions worth calling out

- **`helpdesk_core` as a static library.** Separating `main()` (in
  `app/`) from everything else (in `src/`) is what lets test binaries
  link against production logic without a second `main`.
- **Password masking is split into a testable core.** `PasswordInput::applyKey`
  (insert/backspace/enter handling) has zero terminal dependency and is
  unit tested directly; `readMaskedPassword` is the thin, largely
  untestable `termios` I/O wrapper around it. This was the only
  practical way to get real automated coverage of masking logic without
  a pseudo-terminal.
- **Resolved-ticket filtering lives in SQL, once.** `getActiveTicketsByEngineer`
  / `getResolvedTicketsByEngineer` / `getAllResolvedTickets` are the
  single source of truth; `Engineer::showMenu` and `Admin::showMenu`
  both call them rather than each re-implementing a filter.
- **Case-insensitive uniqueness at the column level.** `username` and
  `email` are declared `COLLATE NOCASE UNIQUE` in the schema itself, so
  the constraint holds even if application code changes later — it
  isn't just a pre-insert check that could be bypassed.
- **Self-registration is Employee-only.** Engineer/Admin accounts are
  provisioned by an Admin (`Admin::manageUsersFlow`), matching the
  original console's separation of privileged onboarding from public
  self-service registration.
- **`Ticket::assignTo` validates the ID shape, not existence.** It
  rejects zero/negative IDs but doesn't query the users table (that's
  the repository's job). The only place a *nonexistent* engineer could
  be assigned is the Admin console flow, which lists real engineers
  pulled from the database — a user can't type an arbitrary ID there.
  This boundary is intentional and is documented/tested explicitly in
  `tests/system/InvalidInputResilienceTest.cpp`.

## 5. Database Schema

```sql
CREATE TABLE users (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  name     TEXT NOT NULL,
  username TEXT NOT NULL COLLATE NOCASE UNIQUE,
  password TEXT NOT NULL,
  email    TEXT NOT NULL COLLATE NOCASE UNIQUE,
  role     TEXT NOT NULL           -- EMPLOYEE | ENGINEER | ADMIN
);

CREATE TABLE tickets (
  id                    INTEGER PRIMARY KEY AUTOINCREMENT,
  employee_id           INTEGER NOT NULL REFERENCES users(id),
  assigned_engineer_id  INTEGER REFERENCES users(id),
  title                 TEXT NOT NULL,
  description           TEXT,
  ticket_type           TEXT NOT NULL DEFAULT 'OTHER',
  priority              TEXT NOT NULL,  -- LOW | MEDIUM | HIGH | CRITICAL
  status                TEXT NOT NULL,  -- OPEN | ASSIGNED | IN_PROGRESS | RESOLVED | CLOSED
  created_at            TEXT NOT NULL,
  updated_at            TEXT NOT NULL,
  resolution_notes      TEXT,
  feedback_rating       INTEGER DEFAULT 0,
  feedback_comment      TEXT
);

CREATE INDEX idx_tickets_status   ON tickets(status);
CREATE INDEX idx_tickets_engineer ON tickets(assigned_engineer_id);
CREATE INDEX idx_tickets_employee ON tickets(employee_id);
```

A default admin (`admin` / `admin123`) is seeded on first run.
`DatabaseManager::initializeSchema()` also runs a defensive
`ALTER TABLE tickets ADD COLUMN ticket_type ...`, swallowing the
"duplicate column" error if it already exists — a lightweight forward
migration so databases created by earlier revisions of this project
don't break.

## 6. Build Instructions

### Prerequisites

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake libsqlite3-dev \
    libgtest-dev libgmock-dev \
    valgrind cppcheck gcovr lcov
```

(All of the above were actually installed and used to produce every
number in this README — see [§10](#10-testing)–[§14](#14-coverage).)

### Plain build

```bash
make            # configures build/ with CMake and compiles everything
./build/helpdesk
```

or directly with CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -- -j"$(nproc)"
./build/helpdesk
```

### make targets

```bash
make            # build app + all three test binaries
make run        # build, then launch the console app
make unit           # build, run unit tests only
make integration    # build, run integration tests only
make system         # build, run system tests only
make test            # build, run all three suites via ctest
make coverage         # coverage-instrumented build + full run + report
make valgrind          # run the interactive app under Memcheck
make helgrind            # run the interactive app under Helgrind
make valgrind-test        # run all 3 test suites under strict Memcheck
make helgrind-test          # run system tests under Helgrind
make cppcheck                 # static analysis
make sanitize                  # ASan+UBSan build + run all tests
make clean                      # remove all build dirs, logs, .db files
```

## 7. Application Usage

1. `./build/helpdesk` — schema is created automatically on first launch.
2. **Register** a new Employee account, or **Login** as the seeded admin
   (`admin` / `admin123`). Passwords are entered masked (see [§15](#15-security-considerations)).
3. **Admin**: add Engineer accounts (Manage Users → Add User), view all
   tickets, view resolved tickets, assign open tickets to engineers,
   view reports.
4. **Employee**: raise tickets (choosing a ticket type and priority),
   view your own tickets, leave feedback once a ticket is resolved.
5. **Engineer**: view *active* assigned tickets (Assigned / In
   Progress), view your resolved tickets separately, update status,
   resolve tickets with notes.

## 8. Ticket Types

Every ticket is classified at creation time:

| # | Enum value            | Menu label             |
|---|-------------------------|--------------------------|
| 1 | `SOFTWARE`                | Software                  |
| 2 | `HARDWARE`                  | Hardware                    |
| 3 | `INFOSEC`                     | Information Security          |
| 4 | `NETWORK`                       | Network                          |
| 5 | `ACCESS_MANAGEMENT`               | Access Management                   |
| 6 | `DATABASE`                          | Database                              |
| 7 | `APPLICATION_SUPPORT`                 | Application Support                     |
| 8 | `OTHER`                                 | Other                                     |

Stored in `tickets.ticket_type` as the canonical upper-case token (e.g.
`ACCESS_MANAGEMENT`); an unrecognized token anywhere in the pipeline
throws `std::invalid_argument` rather than silently defaulting, so a
corrupted or hand-edited row is surfaced, not swallowed.

## 9. Validation Rules

### Username

- Allowed characters: letters, digits, `.`, `_`
- Length: 3–32 characters
- Uniqueness: **case-insensitive** (`somesh`, `Somesh`, `SOMESH` all
  collide), enforced by `UNIQUE(username COLLATE NOCASE)` at the schema
  level, not just in application code

### Email

- Exactly one `@`
- Non-empty local part and domain
- Domain contains at least one `.`
- No empty labels (no leading/trailing/consecutive dots)
- No embedded or leading/trailing whitespace
- Uniqueness: one account per email address, also case-insensitive at
  the schema level

Both are implemented in `Validation.h/.cpp` (stateless, no DB
dependency) and called from **two** places for defense-in-depth: the
console layer (fast feedback) and `DatabaseManager::addUser` (so no
caller can ever bypass the rule).

### Password

- Never displayed in plaintext; each keystroke echoes as `*`
- Never logged
- Empty passwords are rejected (`readMaskedNonEmptyPassword` re-prompts)

## 10. Testing

**109 tests, 3 suites, 100% passing** (verified via `make test`):

| Suite        | Count | What it covers |
|---------------|-------|------------------|
| Unit           | 64    | `Ticket` behavior, `Validation` (every example from the spec, literally), `PasswordInput` keystroke logic, `DatabaseManager` repository CRUD/filtering |
| Integration      | 24    | Authentication, ticket persistence, ticket-type persistence (parametrized across all 8 types), resolution workflow, resolved-ticket filtering, duplicate username/email constraints — each against its own throwaway SQLite file |
| System             | 21    | Full lifecycle with a simulated service restart, ticket-type matrix in one session, invalid-input resilience (bad ticket types, unknown IDs, illegal status transitions, garbage menu input), **and console-menu-level tests that drive the real `Employee`/`Engineer`/`Admin::showMenu()` loops via redirected stdin/stdout** |

```bash
make unit
make integration
make system
make test          # all three, with a label summary
```

Every integration/system test gets its own uniquely-named SQLite file
(`tests/fixtures/TestDatabaseFixture.h`), created fresh in `SetUp()` and
deleted in `TearDown()`. **The production `helpdesk.db` is never touched
by any test.**

## 11. Diagnostics (Valgrind / Helgrind)

```bash
make valgrind          # interactive app under Memcheck (attach a real terminal)
make helgrind             # interactive app under Helgrind
make valgrind-test           # all 3 suites, strict settings, logs to build/logs/
make helgrind-test              # system suite under Helgrind
```

**Actual results from this build** (`build/logs/valgrind-*.log`):

```
valgrind-unit.log:        ERROR SUMMARY: 0 errors from 0 contexts
valgrind-integration.log: ERROR SUMMARY: 0 errors from 0 contexts
valgrind-system.log:      ERROR SUMMARY: 0 errors from 0 contexts
```
All three logs also report `All heap blocks were freed -- no leaks are
possible` (0 bytes in use at exit, allocs == frees across ~13k–20k
allocations per suite).

`helgrind-test` (`build/logs/helgrind-system.log`): `ERROR SUMMARY: 0
errors`. **Note:** this application is single-threaded end-to-end, so a
clean Helgrind run is the expected (and only possible) outcome — the
target exists so any future concurrency work is covered from day one,
not because races were found and fixed.

## 12. Static Analysis (Cppcheck)

```bash
make cppcheck    # writes build/logs/cppcheck.log and prints it
```

Run with `--enable=all --inconclusive --std=c++17` against `src/`,
`include/`, and `app/`. No MISRA checking is configured or claimed. The
current report is clean of correctness/bug-prone findings; the
remaining notes are all `style`/`performance`/`inconclusive`
observations (e.g. "these accessor methods could be `const`", "these
console-menu methods could be `static`", a couple of "prefer
`std::any_of`/`std::all_of` over a raw loop" suggestions in
`Validation.cpp`, and a handful of methods cppcheck considers unused
because it only analyzes `src/`+`app/`, not `tests/`, where some
repository methods like `getUserById` are in fact exercised).

## 13. Sanitizers

```bash
make sanitize    # separate build-sanitize/ tree, -fsanitize=address,undefined
```

Compiled and linked with `-fsanitize=address,undefined
-fno-omit-frame-pointer -g -O0`. All 109 tests pass under this build
with **zero** ASan/UBSan reports — any real memory error or undefined
behavior would abort the process and fail the corresponding test
immediately, so a clean `ctest` run here is a strong (not just
best-effort) signal.

## 14. Coverage

```bash
make coverage    # build-coverage/ tree, runs all 3 suites, then gcovr
```

**Actual measured coverage of `src/` + `include/`** (via `gcovr`, HTML
report at `build-coverage/coverage-report/index.html`):

```
lines:     83.7% (817 / 976)
functions: 95.2% (99 / 104)
branches:  72.6% (866 / 1193)
```

**Honesty note on the 90% / 90% / 80% gates requested in the original
spec:** functions exceed the 90% gate; lines and branches land close
but below it (83.7% and 72.6% respectively). I'm reporting the real
numbers rather than claiming the gates are met. The gap is
concentrated in exactly the places you'd expect and none of it is in
the areas the spec calls out for maximum coverage:

- **Validation.cpp: 100%**, **Ticket.cpp: 98%**, **Ticket.h: 100%** —
  the areas the original spec explicitly asked to target for maximum
  coverage (validation, ticket filtering/resolution) are effectively
  fully covered.
- **DatabaseManager.cpp: 84%** — the uncovered lines are almost
  entirely `sqlite3_prepare_v2`/`sqlite3_step` failure branches (e.g.
  "the statement failed to compile"), which would require deliberately
  corrupting the SQLite library or schema mid-test to trigger; not
  exercised for that reason.
- **User.cpp: 81%** — driven up substantially (from 0%) by
  `ConsoleMenuWorkflowTest.cpp`, which redirects stdin/stdout to run
  real menu sessions. The remaining gaps are mostly alternate
  menu-choice branches (e.g. status-update option 1 vs. option 2) not
  yet scripted.
- **PasswordInput.cpp: 58%** — the actual `termios`/`isatty` raw-mode
  branch cannot run under CI without a real pseudo-terminal (there is
  no controlling TTY in this environment or in typical CI runners), so
  only the non-TTY fallback path is exercised by automated tests. The
  masking *logic itself* (`classifyKey`/`applyKey`) is unit tested
  directly and is 100% covered — see [§10](#10-testing). A pty-based
  test is listed under Future Enhancements.
- **InputUtil.h: 65%** — several "invalid choice, try again" retry
  branches in the ticket-type/priority menu helpers aren't yet
  exercised by a dedicated test.

## 15. Security Considerations

- **Password masking**: real character-by-character terminal I/O via
  `<termios.h>` (never `getpass()`), echo/canonical/signal-generation
  disabled for the duration of entry only, and unconditionally restored
  via an RAII guard on every exit path (including exceptions).
- **No plaintext password logging**: passwords are never written to any
  log or console output; only `*` is echoed.
- **Prepared statements everywhere**: every user-supplied value that
  reaches SQL goes through `sqlite3_prepare_v2` + `sqlite3_bind_*`.
  There is no string-concatenated SQL anywhere in the codebase.
- **Username/email constraints enforced at the schema level**
  (`UNIQUE ... COLLATE NOCASE`), not just in application code, so the
  guarantee holds even if a future caller forgets to pre-validate.
- **Known limitation, stated plainly**: passwords are stored as
  plaintext in the `users.password` column. This mirrors the original
  prototype and keeps the demo simple; a production system must hash +
  salt (e.g. bcrypt/Argon2) before storing. This is flagged rather than
  silently left implicit.

## 16. File-by-File Change Summary

| File | Change |
|------|--------|
| `include/Common.h` | Added `TicketType` enum + string/label conversions |
| `include/Validation.h` / `src/Validation.cpp` | **New.** Username/email format validation |
| `include/PasswordInput.h` / `src/PasswordInput.cpp` | **New.** Masked password input (termios), testable keystroke core |
| `include/Ticket.h` / `src/Ticket.cpp` | Added `TicketType` field end-to-end (constructors, getter, display) |
| `include/DatabaseManager.h` / `src/DatabaseManager.cpp` | `ticket_type` column + migration; `COLLATE NOCASE UNIQUE` on username/email; friendly constraint-violation translation; `getActiveTicketsByEngineer` / `getResolvedTicketsByEngineer` / `getAllResolvedTickets`; indexes |
| `include/User.h` / `src/User.cpp` | Ticket-type selection in ticket creation; masked password prompts; validation calls; new "View Resolved Tickets" menu items (Engineer + Admin); `Engineer`/`Admin` "Assigned" views now query the active-only repository methods |
| `include/InputUtil.h` | Added `readTicketType()` |
| `app/main.cpp` | Moved out of `src/` into its own `app/` dir (so `src/` is a pure library); masked password + validation on login/registration; **[Bug 2]** `sqlite3_config(SQLITE_CONFIG_SINGLETHREAD)` as the first statement of `main()` |
| `src/PasswordInput.cpp` | **[Bug 1]** Raw-mode read loop now sources characters via `std::cin.get(ch)` instead of a raw `read(STDIN_FILENO, ...)` syscall, so redirected/scripted `std::cin` in tests is honored instead of ignored |
| `tests/test_main.cpp` | **[Bug 2]** Same `sqlite3_config(SQLITE_CONFIG_SINGLETHREAD)` call, first statement of `main()`, before any fixture constructs a `DatabaseManager` |
| `CMakeLists.txt` | Rewritten: `helpdesk_core` static lib, `helpdesk` exe, 3 GTest binaries. **[Bug 4]** GTest discovery is now `find_package(GTest CONFIG QUIET)` → `find_package(GTest MODULE QUIET)` → `FetchContent` (last resort only), with target-name/GMock-availability resolved once per mode; coverage/sanitizer build options unchanged |
| `Makefile` | Rewritten: `unit`/`integration`/`system`/`test`/`coverage`/`valgrind`/`helgrind`/`valgrind-test`/`helgrind-test`/`cppcheck`/`sanitize`/`clean`. **[Bug 3]** `sanitize` target now probes for a working ASan/UBSan runtime against the resolved compiler before the real build and fails fast with a named-culprit diagnostic instead of a raw linker error |
| `tests/` | **New.** Full unit/integration/system suite (109 tests) + shared fixture + test main |

## 17. Validation Checklist

- [x] Secure masked password input (termios, RAII-restored, no `getpass()`)
- [x] Ticket type classification (8 categories, persisted, displayed, survives restart)
- [x] Resolved-ticket workflow separation (query-level filtering, dedicated view, verified via both DB-layer and real menu-driven tests)
- [x] Username validation (format + case-insensitive DB uniqueness)
- [x] Email validation (format + DB uniqueness)
- [x] GoogleTest via `find_package(GTest CONFIG REQUIRED)` (not FetchContent)
- [x] Unit / Integration / System test directory structure, all linking `helpdesk_core`
- [x] 109 tests, 100% passing (`make test`)
- [x] Valgrind clean: 0 errors, 0 leaks, all 3 suites (`make valgrind-test`)
- [x] Helgrind clean: 0 errors (`make helgrind-test`; single-threaded app)
- [x] Cppcheck integrated (`make cppcheck`); no correctness findings
- [x] ASan+UBSan integrated (`make sanitize`); 109/109 pass under instrumentation
- [x] Coverage integrated (`make coverage`); **actual** numbers reported: 83.7% lines / 95.2% functions / 72.6% branches (not claimed as meeting the 90/90/80 gates — see [§14](#14-coverage) for the honest breakdown)
- [x] Comprehensive README (this document)
- [ ] 90% line / 90% function / 80% branch coverage gates — **functions met (95.2%); lines and branches close but not met (83.7%, 72.6%)**
- [x] Bug 1 fixed: `PasswordInput` raw-mode reads via `std::cin.get()`, honoring redirected input in tests (see [§18](#18-real-machine-bugfix-pass))
- [x] Bug 2 fixed: `SQLITE_CONFIG_SINGLETHREAD` set before first SQLite use in both `app/main.cpp` and `tests/test_main.cpp` (see [§18](#18-real-machine-bugfix-pass))
- [x] Bug 3 fixed: `make sanitize` fails fast with a diagnostic when the ASan/UBSan runtime is missing, instead of a raw linker error (see [§18](#18-real-machine-bugfix-pass))
- [x] Bug 4 fixed: CMake GTest discovery now resilient across CONFIG- and MODULE-packaged distros, `FetchContent` as last resort only (see [§18](#18-real-machine-bugfix-pass))
- [ ] Full `make test` / `make valgrind-test` / `make helgrind-test` / `make cppcheck` / `make sanitize` / `make coverage` re-run on a machine with the complete toolchain to confirm the above end-to-end and refresh the §10-§14 numbers — **not performed in the sandbox this bugfix pass was written in** (no `cmake`/GoogleTest/Valgrind/Cppcheck/gcovr installed, no network access; see [§18](#18-real-machine-bugfix-pass) for exactly what was verified instead)

## 18. Real-Machine Bugfix Pass

Four bugs surfaced during real-machine testing across different
environments (not the Ubuntu 24.04/GCC 13 box the numbers in §10-§14
above were measured on). All four are fixed in this revision. The
figures in §10-§14 are the last full clean-tree run and are **not**
re-claimed here — the environment this bugfix pass was applied in does
not have `cmake`, GoogleTest, Valgrind, Cppcheck, or gcovr installed, and
has no network access to install them, so the full `make test` /
`make valgrind-test` / `make helgrind-test` / `make cppcheck` /
`make sanitize` / `make coverage` matrix could not be re-run end-to-end
here. Each fix below was instead verified as precisely as this
environment allows (`g++ -fsyntax-only` against the real header
dependency graph where headers were available, `make -n` for Makefile
logic, and a live run of the new ASan-probe shell logic on this
machine's actual toolchain) — see the fix-by-fix notes for exactly what
was and wasn't exercised. **Re-run the full verification matrix in
§10-§14 on a machine with the full toolchain before trusting new
numbers.**

**Bug 1 — `PasswordInput::readMaskedPassword()` ignored redirected
`std::cin` on a real TTY.** The raw-terminal branch called `read()`
directly on `STDIN_FILENO`, bypassing whatever streambuf was attached to
`std::cin` — so tests that redirect `std::cin.rdbuf()` to script a
password were silently ignored, and the process blocked on real
keystrokes instead. Fixed by reading through `std::cin.get(ch)` instead
of the raw fd `read()` call inside the existing raw-mode branch; the
`isatty()` gate and the `TerminalRawGuard` termios toggling are
unchanged, so real interactive behavior (masked `*` echo on a genuine
terminal) is identical, but the character *source* now honors test
redirection. Verified with `g++ -std=c++17 -fsyntax-only` against
`src/PasswordInput.cpp` in isolation (no SQLite dependency) — clean.
Full `PasswordInputTest`/`PasswordInputReadTest` re-run under `ctest`
still needs to happen on a machine with GoogleTest installed.

**Bug 2 — Helgrind false-positive "recursive lock" reports against
SQLite's internal mutex bookkeeping**, and a correspondingly slow
`make helgrind-test`. This application is single-threaded end-to-end and
never shares a `DatabaseManager`/connection across threads, so
`sqlite3_config(SQLITE_CONFIG_SINGLETHREAD)` is added as the first
statement of `main()` in both `app/main.cpp` and `tests/test_main.cpp`
(before any `DatabaseManager` touches SQLite, since `sqlite3_config()`
only succeeds prior to SQLite's implicit first-use initialization). This
tells SQLite to skip its internal mutex machinery entirely, which both
eliminates the false positives at the source and should make Helgrind
runs meaningfully faster (less bookkeeping to trace, not just fewer
reports). Verified with `g++ -fsyntax-only` against both files (using a
hand-written stub `sqlite3.h` matching the real API surface this project
actually calls, since no `libsqlite3-dev` is installed in this
environment) — clean. A real `make helgrind-test` re-run to confirm the
~70 prior findings are gone still needs a machine with Valgrind
installed.

**Bug 3 — `make sanitize` fails to link (`cannot find -lasan`) on
toolchains that accept `-fsanitize=address` at compile time but don't
ship the matching runtime** (observed via an alternate `gcc-toolset`-
style install). The `sanitize` Makefile target now compiles+links a
trivial `-fsanitize=address,undefined` probe program against the
resolved compiler (`$(CXX)`, defaulting to `g++`, matching what would be
passed to CMake) before attempting the real instrumented build. On
success it proceeds exactly as before; on failure it prints the resolved
compiler path, the raw probe output, and two concrete remediations
(install the matching sanitizer runtime package, or rebuild with
`CXX=/usr/bin/g++` / `-DCMAKE_CXX_COMPILER=/usr/bin/g++`), then exits
non-zero — so a broken sanitizer toolchain fails loudly instead of being
silently skipped. Verified two ways on this machine: `make -n sanitize`
to confirm the Makefile parses and the recipe expands correctly, and a
live run of the exact probe shell logic — this sandbox's stock Ubuntu
`g++` does have a working ASan/UBSan runtime, so the probe reports
success here, which is the correct "toolchain is fine" outcome (this
environment never reproduced Bug 3 itself; the fix is defensive/general
per the task's own instructions, and is unconditional so it protects any
future toolchain, not just the one that surfaced it).

**Bug 4 — CMake configure fails outright (`Could not find a package
configuration file provided by GTest`) on distros whose GTest package
ships only headers + static libs with no CMake package-config file**
(EL8-family systems being the common case). `CMakeLists.txt` now tries
`find_package(GTest CONFIG QUIET)` first (covers Ubuntu-style
`libgtest-dev`/`libgmock-dev`), falls back to `find_package(GTest MODULE
QUIET)` (CMake's bundled `FindGTest`, which needs no distro
package-config file) if that fails, and only falls back further to
`FetchContent` — with an explicit warning, per project policy that
`FetchContent` stays a last resort — if neither local packaging style is
found. The two `find_package` modes expose different target names
(`GTest::gtest`/`GTest::gmock` vs. `GTest::GTest`, no gmock target), so
target selection is resolved once and reused for all three test
binaries; GMock is only required/linked if some test source actually
references it (a `tests/*.cpp` scan; currently none do, so this project
builds correctly even under MODULE mode, which provides no GMock
target). This machine has no `cmake` installed at all (and no network to
install it), so the configure step itself could not be re-run here;
review the diff in `CMakeLists.txt` directly, and re-run `make test` (a
clean-tree `cmake -S . -B build` reaching the "Build files have been
written" line, with no `find_package` errors) on both an Ubuntu-style
box and an EL8-family box before considering this closed end to end.

## 19. Future Enhancements

- Password hashing + salting (bcrypt/Argon2) instead of plaintext storage
- Pty-based (pseudo-terminal) test harness to close the `PasswordInput.cpp`
  raw-terminal-mode coverage gap
- Additional menu-branch tests to close the remaining `User.cpp`/`InputUtil.h`
  coverage gaps and push toward the 90%/80% line/branch gates
- GUI (Qt), email/SMS notifications, web access, AI-based ticket
  categorization, real-time dashboard (carried over from the original
  project roadmap)
