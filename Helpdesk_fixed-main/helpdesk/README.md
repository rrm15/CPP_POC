# IT Helpdesk Ticket Management System

A console-based IT Helpdesk Ticket Management System in modern C++17,
backed by a real SQLite3 database with PBKDF2-HMAC-SHA256 password security,
a GoogleTest-based three-layer test suite (unit / integration / system),
and a diagnostics/coverage toolchain (Valgrind, Helgrind, Cppcheck, ASan/UBSan,
and native GCC gcov coverage).

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Features](#2-features)
3. [Technology Stack](#3-technology-stack)
4. [Architecture](#4-architecture)
5. [Database Schema & Security](#5-database-schema--security)
6. [Build Instructions](#6-build-instructions)
7. [Application Usage](#7-application-usage)
8. [Ticket Types](#8-ticket-types)
9. [Validation Rules](#9-validation-rules)
10. [Testing](#10-testing)
11. [Diagnostics (Valgrind / Helgrind)](#11-diagnostics-valgrind--helgrind)
12. [Static Analysis (Cppcheck)](#12-static-analysis-cppcheck)
13. [Sanitizers](#13-sanitizers)
14. [Coverage (Native GCC gcov)](#14-coverage-native-gcc-gcov)
15. [Security Considerations](#15-security-considerations)
16. [File-by-File Change Summary](#16-file-by-file-change-summary)
17. [Validation Checklist](#17-validation-checklist)
18. [Future Enhancements](#18-future-enhancements)

---

## 1. Project Overview

The system lets employees raise IT support tickets, administrators
assign them to engineers, and engineers work them through to
resolution — with every action persisted in SQLite so nothing is lost
between sessions. The system features masked password entry, PBKDF2-HMAC-SHA256
password hashing and verification, ticket categorization, a corrected resolved-ticket
workflow, real input validation with database-level enforcement, concurrent-write
safeguards, and an automated test/diagnostics pipeline.

## 2. Features

- Role-based console menus: **Employee**, **Engineer**, **Admin**
  (polymorphic dispatch through an abstract `User` base class)
- Ticket lifecycle: `OPEN → ASSIGNED → IN_PROGRESS → RESOLVED → CLOSED`
- **Ticket type classification** (8 categories, see [§8](#8-ticket-types))
- **Masked password input** (`*` echoed, real terminal raw-mode handling)
- **PBKDF2-HMAC-SHA256 password storage & verification** (NIST-compliant, random salted, constant-time comparison)
- **Resolved-ticket workflow separation** ("Assigned Tickets" no longer
  shows resolved/closed tickets; a dedicated "View Resolved Tickets"
  screen is provided for Engineer and Admin)
- **Username & email validation**, enforced both in the console layer
  and at the database layer (`UNIQUE ... COLLATE NOCASE`)
- SQLite persistence via parameterized queries (`sqlite3_prepare_v2` +
  `sqlite3_bind_*` everywhere — no string-concatenated SQL)
- Atomic conditional updates for assignment, status transition, and resolution
- Reporting: ticket counts by status/priority, average feedback rating
- **Admin Operational Metrics CSV export** (RFC 4180, collision-safe `_N.csv` suffixing)
- **One-time persistent login notifications** per role (Admin/Engineer/Employee)
- Dynamic coverage report with WEIGHTED TOTAL and SIMPLE AVERAGE columns

## 3. Technology Stack

| Component        | Choice                                   |
|-------------------|-------------------------------------------|
| Language           | C++17                                     |
| Database            | SQLite3 (C API, prepared statements)      |
| Cryptography        | OpenSSL `libcrypto` (PBKDF2-HMAC-SHA256, RAND_bytes, CRYPTO_memcmp) |
| Build system         | CMake ≥ 3.16 (+ a Makefile wrapper)      |
| Testing              | GoogleTest / GoogleMock (via `find_package(GTest)`) |
| Memory/thread diagnostics | Valgrind (Memcheck) / Helgrind      |
| Static analysis       | Cppcheck                                |
| Sanitizers             | AddressSanitizer + UndefinedBehaviorSanitizer |
| Coverage                | Native GCC `gcov` terminal reporting     |

## 4. Architecture

```
helpdesk/
├── include/                  Headers (declarations only)
│   ├── Common.h                Enums (UserRole, TicketStatus, TicketPriority,
│   │                           TicketType) + string conversions + custom exceptions
│   ├── Validation.h             Username/email format validation (pure, no DB dependency)
│   ├── PasswordInput.h          Masked password entry (termios), testable keystroke core
│   ├── PasswordHasher.h         PBKDF2-HMAC-SHA256 hashing and verification interface
│   ├── DateUtil.h                Timestamp helper
│   ├── InputUtil.h                Console input helpers (readInt, menus, etc.)
│   ├── Ticket.h                    Ticket entity (encapsulated domain model)
│   ├── User.h                      Abstract User base + Employee/Engineer/Admin
│   └── DatabaseManager.h            SQLite data-access layer (repository with atomic writes)
├── src/                       Implementations of everything above
│   (compiled into the helpdesk_core static library — no main())
├── app/
│   └── main.cpp                The only file with main(); login/registration flow
├── tests/
│   ├── fixtures/
│   │   └── TestDatabaseFixture.h   Per-test throwaway SQLite file (never production DB)
│   ├── unit/                  Ticket, Validation, PasswordInput, PasswordHasher, DatabaseManager unit tests
│   ├── integration/           Multi-step scenarios against a real temporary SQLite database
│   ├── system/                Full end-to-end workflows and interactive console-menu tests
│   └── test_main.cpp          Shared GoogleTest entry point
├── CMakeLists.txt             helpdesk_core lib + helpdesk exe + 3 test binaries
├── Makefile                   make unit/integration/system/test/coverage/coverage-summary/...
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

## 5. Database Schema & Security

```sql
CREATE TABLE users (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  name     TEXT NOT NULL,
  username TEXT NOT NULL COLLATE NOCASE UNIQUE,
  password TEXT NOT NULL,          -- pbkdf2_sha256$<iterations>$<salt-hex>$<hash-hex>
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

### Password Storage & Authentication
- **PBKDF2-HMAC-SHA256**: All user passwords are encrypted using PBKDF2 with HMAC-SHA256, 100,000 iterations, a fresh cryptographically random 16-byte salt (`RAND_bytes`), and 32-byte key derivation.
- **Storage Format**: `pbkdf2_sha256$<iterations>$<salt-hex>$<hash-hex>`
- **Verification**: Verified in C++ via constant-time comparison (`CRYPTO_memcmp`) to protect against timing attacks. Plaintext is never stored, logged, or compared in SQL.
- **Seeded Admin**: The default administrator account (`admin` / `admin123`) is automatically seeded with a hashed PBKDF2 password upon initial schema creation.
- **Legacy Databases**: Legacy databases storing plaintext passwords require database recreation or account password reset.

## 6. Build Instructions

### Prerequisites

**RHEL / Fedora / CentOS**:
```bash
sudo dnf install -y gcc-c++ cmake sqlite-devel openssl-devel gtest-devel
```

**Ubuntu / Debian**:
```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake libsqlite3-dev libssl-dev \
    libgtest-dev libgmock-dev valgrind cppcheck
```

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
make coverage         # coverage build + full test run + native gcov terminal report
make coverage-summary # reprint existing coverage report without rebuilding
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

## 9. Validation Rules

### Username

- Allowed characters: letters, digits, `.`, `_`
- Length: 3–32 characters
- Uniqueness: **case-insensitive** (`somesh`, `Somesh`, `SOMESH` all collide), enforced by `UNIQUE(username COLLATE NOCASE)`

### Email

- Exactly one `@`
- Non-empty local part and domain
- Domain contains at least one `.`
- No empty labels (no leading/trailing/consecutive dots)
- No embedded or leading/trailing whitespace
- Uniqueness: case-insensitive at schema level

### Password

- Masked character entry with `*` echo via `<termios.h>`
- Validated for non-empty input
- Hashed with PBKDF2-HMAC-SHA256 prior to persistence
- Never displayed or logged in plaintext

## 10. Testing

**150+ tests across 3 suites, 100% passing** (`make test`):

| Suite        | Count | What it covers |
|---------------|-------|------------------|
| Unit           | 85+   | `Ticket` behavior, `Validation`, `PasswordInput` keystroke logic, `PasswordHasher` PBKDF2 unit tests, `DatabaseManager` repository CRUD/filtering, **`CsvReportWriter` RFC 4180 escaping and collision suffixing** |
| Integration      | 50+   | Authentication, PBKDF2 persistence, concurrent-write races, duplicate constraints, resolution workflows, **login notification flows for Admin/Engineer/Employee, acknowledgment persistence across DB reopen** |
| System             | 21    | Full lifecycle with service restart, ticket-type matrix in one session, invalid-input resilience, and console-menu-level tests driving `Employee`/`Engineer`/`Admin::showMenu()` |

```bash
make test
```

## 11. Diagnostics (Valgrind / Helgrind)

```bash
make valgrind          # interactive app under Memcheck
make valgrind-test        # all 3 suites under strict Memcheck
make helgrind-test          # system suite under Helgrind
```

## 12. Static Analysis (Cppcheck)

```bash
make cppcheck    # writes build/logs/cppcheck.log and prints it
```

## 13. Sanitizers

```bash
make sanitize    # separate build-sanitize/ tree, -fsanitize=address,undefined
```

## 14. Coverage (Native GCC gcov)

```bash
make coverage         # full: clean instrumented build → run all suites → print table + write CSV
make coverage-summary # reprint table and update CSV from existing build-coverage/ data
```

Coverage is generated using native GCC `gcov` (`gcov -n`) without external script dependencies or HTML generators.
The `coverage-summary` target **dynamically discovers** all compiled production `.cpp` files in
`build-coverage/CMakeFiles/helpdesk_core.dir/` at report time:

```text
============================================================
HELPDESK COVERAGE REPORT
============================================================
SOURCE FILE                      EXECUTED  TOTAL   COVERAGE
------------------------------------------------------------
CsvReportWriter.cpp                    58     82     70.73%
DatabaseManager.cpp                   421    594     70.88%
DateUtil.cpp                            8     16     50.00%
InputUtil.cpp                          31     48     64.58%
PasswordHasher.cpp                     66     71     92.96%
PasswordInput.cpp                      36     60     60.00%
Ticket.cpp                             84     85     98.82%
User.cpp                              196    250     78.40%
Validation.cpp                         44     44    100.00%
------------------------------------------------------------
WEIGHTED TOTAL                        944   1250     75.52%
SIMPLE AVERAGE                                       76.26%
============================================================
Coverage CSV: build/reports/coverage.csv
```

The CSV (`build/reports/coverage.csv`) has one data row per source file plus summary rows.
No new build files need to be added when a new `.cpp` is added to `helpdesk_core` — discovery is automatic.

## 15. Security Considerations

- **PBKDF2-HMAC-SHA256 Password Storage**: Passwords are never stored in plaintext. Passwords are salted with a fresh 16-byte random salt from `RAND_bytes()` and hashed using 100,000 iterations of PBKDF2-HMAC-SHA256.
- **Constant-Time Verification**: Verification uses OpenSSL's `CRYPTO_memcmp()` to protect against timing attacks.
- **Generic Authentication Failures**: Failed logins return generic `"Invalid username or password."` messages without disclosing username existence.
- **Masked Terminal Input**: Masked password entry with `*` echo via `<termios.h>` with RAII terminal mode restoration.
- **Prepared Statements Everywhere**: All dynamic SQL queries use `sqlite3_prepare_v2` and `sqlite3_bind_*` to prevent SQL injection.
- **Atomic Conditional Updates**: State transitions (assignment, progress updates, resolution) use conditional SQL WHERE clauses and inspect `sqlite3_changes()` to guard against concurrent write races.

## 16. File-by-File Change Summary

| File | Change |
|------|--------|
| `include/PasswordHasher.h` / `src/PasswordHasher.cpp` | **New.** PBKDF2-HMAC-SHA256 password hashing and verification using OpenSSL `libcrypto`. |
| `include/Common.h` | Custom exception hierarchy including `LockConflictException` and updated `AuthenticationException`. |
| `include/DatabaseManager.h` / `src/DatabaseManager.cpp` | Prepared statements, `sqlite3_open_v2` with `SQLITE_OPEN_FULLMUTEX`, atomic conditional updates, PBKDF2 integration, seeded admin hash. **New:** `user_notification_state` table schema, `getPendingNotifications`, `acknowledgeNotifications`, `recordInitialNotificationState`. |
| `include/DateUtil.h` / `src/DateUtil.cpp` | **New.** Shared timestamp helpers (`nowString`, `nowFilenameTimestamp`). |
| `include/InputUtil.h` / `src/InputUtil.cpp` | **New.** Console input helpers split into separate translation unit. |
| `include/CsvReportWriter.h` / `src/CsvReportWriter.cpp` | **New.** `OperationalMetrics` struct + RFC 4180 CSV writer with collision-safe `_N.csv` suffixing and `fs::create_directories`. |
| `include/User.h` / `src/User.cpp` | `User::checkPassword()` + `Admin::reportsFlow` now captures `OperationalMetrics`, displays terminal table, exports CSV via `CsvReportWriter`. |
| `app/main.cpp` | Masked login/registration + **login notification display and acknowledgment** in `loginFlow`. |
| `CMakeLists.txt` | Added `OpenSSL::Crypto`, `DateUtil.cpp`, `InputUtil.cpp`, `CsvReportWriter.cpp` to `helpdesk_core`. |
| `Makefile` | Dynamic `coverage-summary` with EXECUTED/TOTAL/COVERAGE columns, WEIGHTED TOTAL, SIMPLE AVERAGE, and `build/reports/coverage.csv` export. |
| `tests/unit/PasswordHasherTest.cpp` | **New.** Comprehensive unit tests for PBKDF2. |
| `tests/unit/CsvReportWriterTest.cpp` | **New.** RFC 4180 escaping, collision suffixing, file writing, error cases. |
| `tests/integration/AuthenticationIntegrationTest.cpp` | Updated with PBKDF2 persistence checks. |
| `tests/integration/LoginNotificationIntegrationTest.cpp` | **New.** Admin/Engineer/Employee notification flows, acknowledgment persistence across DB reopen. |

## 17. Validation Checklist

- [x] PBKDF2-HMAC-SHA256 password storage and verification
- [x] OpenSSL libcrypto integration (`PKCS5_PBKDF2_HMAC`, `EVP_sha256`, `RAND_bytes`, `CRYPTO_memcmp`)
- [x] Seeded Admin password hashed with PBKDF2
- [x] Masked password entry preserved (`PasswordInput`)
- [x] Prepared statements for all dynamic SQL operations
- [x] Safe SQLite initialization (`sqlite3_open_v2` + `SQLITE_OPEN_FULLMUTEX`)
- [x] Dynamic `gcov` coverage terminal report (EXECUTED / TOTAL / COVERAGE + WEIGHTED TOTAL + SIMPLE AVERAGE)
- [x] `build/reports/coverage.csv` written by `make coverage` / `make coverage-summary`
- [x] Admin Option 5 (Reports & Statistics) exports `build/reports/admin/operational_metrics_*.csv`
- [x] Login notifications (Admin: unassigned OPEN tickets; Engineer: newly assigned; Employee: status changes)
- [x] Notification acknowledgment persists in `user_notification_state` across DB reopens
- [x] 150+ tests passing across unit, integration, and system suites
- [x] Valgrind / ASan / UBSan / Cppcheck compatibility maintained

## 18. Future Enhancements

- Pty-based (pseudo-terminal) test harness for raw-terminal termios testing.
- GUI / Web interface options.
- Email/SMS notifications for ticket status updates.
