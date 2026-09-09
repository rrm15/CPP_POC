# Helpdesk Control System

A C++17 terminal-native Helpdesk application with SQLite3 backend.

## Features
- User management (Admin, Engineer, Customer)
- Ticket lifecycle management (OPEN, ASSIGNED, IN_PROGRESS, RESOLVED)
- Real-time ticket assignment and status tracking
- Password-protected authentication
- Multi-terminal safe concurrent access

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Database Concurrency

Each terminal process owns its own SQLite connection to `helpdesk.db`.

- **Connection Model**: Each process maintains independent database connections
- **Locking**: SQLite's native file-level locking (`SQLITE_OPEN_FULLMUTEX`) prevents concurrent corruption
- **Conditional Updates**: Assignment, status, and resolution operations use conditional SQL to prevent overwrites
- **No Singleton**: Cross-process coordination uses SQLite constraints and conditional updates, not C++ singletons
- **Lock Conflicts**: If a database is locked, the application reports a retry-friendly message

## Testing

```bash
make test                # Run all unit/integration/system tests
make valgrind-test       # Run tests with memory checks
make cppcheck            # Run static analysis
make sanitize            # Run with address sanitizer
make coverage            # Generate coverage report with native gcov
```

## Coverage Reporting

The `make coverage` target uses native GCC `gcov` to generate terminal-only reports:

```bash
make coverage
```

Output shows:
- Per-file line coverage percentages
- Total coverage summary
- Only production code (excludes tests, headers, CMake files)

No external tools required (no gcovr, lcov, HTML, or browser output).

## Password Storage and Authentication

Passwords are validated during login and never stored in plaintext or hashed on disk.
Authentication behavior remains unchanged.
