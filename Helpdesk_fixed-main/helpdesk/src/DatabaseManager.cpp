#include "DatabaseManager.h"
#include "DateUtil.h"
#include "Validation.h"
#include <iostream>
#include <cstring>

// Maps SQLITE_BUSY / SQLITE_LOCKED to a LockConflictException so callers
// never have to inspect raw SQLite return codes for lock conditions.
static void throwIfLockConflict(int rc) {
    if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
        throw LockConflictException();
    }
}

DatabaseManager::DatabaseManager(const std::string& path)
    : db(nullptr), dbPath(path) {}

DatabaseManager::~DatabaseManager() {
    close();
}

void DatabaseManager::connect() {
    // SQLITE_OPEN_FULLMUTEX: serialized threading mode -- SQLite serializes
    // all database access through a single mutex per connection. Required
    // even in single-process use when multiple logical sessions share state
    // via separate DatabaseManager instances on the same file.
    int rc = sqlite3_open_v2(
        dbPath.c_str(),
        &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );
    if (rc != SQLITE_OK) {
        // Capture the error before closing; sqlite3_errmsg() is only valid
        // while the handle is open.
        std::string msg = db ? sqlite3_errmsg(db) : "unknown error";
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        throw DatabaseException("Could not open database '" + dbPath + "': " + msg);
    }
    // Enforce foreign key constraints (off by default in SQLite).
    // This must succeed; a failure here indicates a misconfigured build.
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        db = nullptr;
        throw DatabaseException("PRAGMA foreign_keys = ON failed: " + msg);
    }
}

void DatabaseManager::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool DatabaseManager::beginTransaction() {
    execute("BEGIN IMMEDIATE;");
    return true;
}

bool DatabaseManager::commit() {
    execute("COMMIT;");
    return true;
}

bool DatabaseManager::rollback() {
    execute("ROLLBACK;");
    return true;
}

void DatabaseManager::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw DatabaseException("Failed executing statement: " + msg);
    }
}


void DatabaseManager::initializeSchema() {
    try {
        // username/email use COLLATE NOCASE so the column's own UNIQUE
        // index enforces case-insensitive uniqueness at the database
        // level ("somesh" / "Somesh" / "SOMESH" collide) -- this is the
        // authoritative constraint; validateUsernameOrThrow/addUser's
        // pre-check are UX conveniences, not the source of truth.
        execute(
            "CREATE TABLE IF NOT EXISTS users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL,"
            "  username TEXT NOT NULL COLLATE NOCASE UNIQUE,"
            "  password TEXT NOT NULL,"
            "  email TEXT NOT NULL COLLATE NOCASE UNIQUE,"
            "  role TEXT NOT NULL"
            ");"
        );

        execute(
            "CREATE TABLE IF NOT EXISTS tickets ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  employee_id INTEGER NOT NULL,"
            "  assigned_engineer_id INTEGER,"
            "  title TEXT NOT NULL,"
            "  description TEXT,"
            "  ticket_type TEXT NOT NULL DEFAULT 'OTHER',"
            "  priority TEXT NOT NULL,"
            "  status TEXT NOT NULL,"
            "  created_at TEXT NOT NULL,"
            "  updated_at TEXT NOT NULL,"
            "  resolution_notes TEXT,"
            "  feedback_rating INTEGER DEFAULT 0,"
            "  feedback_comment TEXT,"
            "  FOREIGN KEY(employee_id) REFERENCES users(id),"
            "  FOREIGN KEY(assigned_engineer_id) REFERENCES users(id)"
            ");"
        );

        // Lightweight forward migration: databases created by earlier
        // versions of this project (before ticket_type existed) won't
        // have the column yet. SQLite has no "ADD COLUMN IF NOT EXISTS",
        // so we attempt the ALTER and swallow the specific "duplicate
        // column name" failure that means it's already there.
        try {
            execute("ALTER TABLE tickets ADD COLUMN ticket_type TEXT NOT NULL DEFAULT 'OTHER';");
        } catch (const DatabaseException& ex) {
            std::string msg = ex.what();
            if (msg.find("duplicate column name") == std::string::npos) {
                throw; // a genuine failure, not just "already migrated"
            }
        }

        // Helpful indexes for the query patterns used throughout the
        // repository layer (assignment/status filtering, per-user
        // lookups).
        execute("CREATE INDEX IF NOT EXISTS idx_tickets_status ON tickets(status);");
        execute("CREATE INDEX IF NOT EXISTS idx_tickets_engineer ON tickets(assigned_engineer_id);");
        execute("CREATE INDEX IF NOT EXISTS idx_tickets_employee ON tickets(employee_id);");

        // Seed a default admin account if the users table is empty so the
        // application is usable on first run without manual SQL setup.
        execute(
            "INSERT INTO users (name, username, password, email, role) "
            "SELECT 'System Administrator', 'admin', 'admin123', 'admin@company.com', 'ADMIN' "
            "WHERE NOT EXISTS (SELECT 1 FROM users WHERE username = 'admin');"
        );
    } catch (const DatabaseException& ex) {
        throw DatabaseException(std::string("Schema initialization failed: ") + ex.what());
    }
}

// ---------------------------------------------------------------------
// User operations
// ---------------------------------------------------------------------

int DatabaseManager::addUser(const std::string& name, const std::string& username,
                              const std::string& password, const std::string& email,
                              UserRole role) {
    // Defense-in-depth: re-validate format here too, using the same
    // shared utility the console layer uses, so DatabaseManager never
    // trusts a caller to have already checked (per the requirement to
    // not rely solely on UI-level validation).
    Validation::validateUsernameOrThrow(username);
    Validation::validateEmailOrThrow(email);

    // Friendly pre-check so the common case produces a clean message
    // without needing to parse SQLite error text. The UNIQUE(...
    // COLLATE NOCASE) constraints below are the real, race-safe
    // enforcement mechanism; this is strictly a UX nicety.
    if (usernameExists(username)) {
        throw ValidationException("Username already exists. Please choose another username.");
    }
    if (emailExists(email)) {
        throw ValidationException("Email already exists. Please use a different email address.");
    }

    const char* sql = "INSERT INTO users (name, username, password, email, role) VALUES (?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare addUser statement: ") + sqlite3_errmsg(db));
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, password.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, roleToString(role).c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string msg = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        // Translate the raw SQLite constraint violation (which is the
        // authoritative, race-safe check -- the usernameExists()
        // pre-check above has a theoretical TOCTOU gap) into a friendly,
        // field-specific message.
        if (rc == SQLITE_CONSTRAINT) {
            if (msg.find("users.username") != std::string::npos) {
                throw ValidationException("Username already exists. Please choose another username.");
            }
            if (msg.find("users.email") != std::string::npos) {
                throw ValidationException("Email already exists. Please use a different email address.");
            }
        }
        throw DatabaseException("Failed to insert user: " + msg);
    }
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

bool DatabaseManager::usernameExists(const std::string& username) {
    const char* sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare usernameExists statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    sqlite3_finalize(stmt);
    return exists;
}

bool DatabaseManager::emailExists(const std::string& email) {
    const char* sql = "SELECT COUNT(*) FROM users WHERE email = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare emailExists statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    sqlite3_finalize(stmt);
    return exists;
}

UserRecord DatabaseManager::authenticate(const std::string& username, const std::string& password) {
    const char* sql = "SELECT id, name, username, password, email, role FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare authenticate statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    UserRecord rec{};
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        rec.role = stringToRole(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
    }
    sqlite3_finalize(stmt);

    if (!found) {
        throw AuthenticationException("No such username: " + username);
    }
    if (rec.password != password) {
        throw AuthenticationException("Incorrect password for username: " + username);
    }
    return rec;
}

UserRecord DatabaseManager::getUserById(int id) {
    const char* sql = "SELECT id, name, username, password, email, role FROM users WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getUserById statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_int(stmt, 1, id);

    UserRecord rec{};
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        rec.role = stringToRole(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
    }
    sqlite3_finalize(stmt);

    if (!found) {
        throw NotFoundException("User with ID " + std::to_string(id) + " not found.");
    }
    return rec;
}

std::vector<UserRecord> DatabaseManager::getAllUsers() {
    const char* sql = "SELECT id, name, username, password, email, role FROM users ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getAllUsers statement: ") + sqlite3_errmsg(db));
    }
    std::vector<UserRecord> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord rec;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        rec.role = stringToRole(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        results.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<UserRecord> DatabaseManager::getUsersByRole(UserRole role) {
    const char* sql = "SELECT id, name, username, password, email, role FROM users WHERE role = ? ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getUsersByRole statement: ") + sqlite3_errmsg(db));
    }
    std::string roleStr = roleToString(role);
    sqlite3_bind_text(stmt, 1, roleStr.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<UserRecord> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        UserRecord rec;
        rec.id = sqlite3_column_int(stmt, 0);
        rec.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        rec.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        rec.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        rec.role = role;
        results.push_back(rec);
    }
    sqlite3_finalize(stmt);
    return results;
}

// ---------------------------------------------------------------------
// Ticket operations
// ---------------------------------------------------------------------

int DatabaseManager::addTicket(const Ticket& ticket) {
    const char* sql =
        "INSERT INTO tickets (employee_id, assigned_engineer_id, title, description, "
        "ticket_type, priority, status, created_at, updated_at, resolution_notes, "
        "feedback_rating, feedback_comment) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare addTicket statement: ") + sqlite3_errmsg(db));
    }

    sqlite3_bind_int(stmt, 1, ticket.getEmployeeId());
    if (ticket.getAssignedEngineerId() == -1) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_int(stmt, 2, ticket.getAssignedEngineerId());
    }
    sqlite3_bind_text(stmt, 3, ticket.getTitle().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ticket.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, ticketTypeToString(ticket.getType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, priorityToString(ticket.getPriority()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, statusToString(ticket.getStatus()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, ticket.getCreatedAt().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, ticket.getUpdatedAt().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, ticket.getResolutionNotes().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 11, ticket.getFeedbackRating());
    sqlite3_bind_text(stmt, 12, ticket.getFeedbackComment().c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string msg = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to insert ticket: " + msg);
    }
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

void DatabaseManager::updateTicket(const Ticket& ticket) {
    if (ticket.getId() == -1) {
        throw ValidationException("Cannot update a ticket that has not been persisted yet.");
    }
    const char* sql =
        "UPDATE tickets SET employee_id=?, assigned_engineer_id=?, title=?, description=?, "
        "ticket_type=?, priority=?, status=?, created_at=?, updated_at=?, resolution_notes=?, "
        "feedback_rating=?, feedback_comment=? WHERE id=?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare updateTicket statement: ") + sqlite3_errmsg(db));
    }

    sqlite3_bind_int(stmt, 1, ticket.getEmployeeId());
    if (ticket.getAssignedEngineerId() == -1) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_int(stmt, 2, ticket.getAssignedEngineerId());
    }
    sqlite3_bind_text(stmt, 3, ticket.getTitle().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, ticket.getDescription().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, ticketTypeToString(ticket.getType()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, priorityToString(ticket.getPriority()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, statusToString(ticket.getStatus()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, ticket.getCreatedAt().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, ticket.getUpdatedAt().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, ticket.getResolutionNotes().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 11, ticket.getFeedbackRating());
    sqlite3_bind_text(stmt, 12, ticket.getFeedbackComment().c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, ticket.getId());

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::string msg = sqlite3_errmsg(db);
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to update ticket: " + msg);
    }
    sqlite3_finalize(stmt);
}

// Helper: builds a Ticket object from the current row of a prepared statement.
// Column order must match: id, employee_id, assigned_engineer_id, title,
// description, ticket_type, priority, status, created_at, updated_at,
// resolution_notes, feedback_rating, feedback_comment
static Ticket ticketFromRow(sqlite3_stmt* stmt) {
    int id = sqlite3_column_int(stmt, 0);
    int employeeId = sqlite3_column_int(stmt, 1);
    int assignedEngineerId = (sqlite3_column_type(stmt, 2) == SQLITE_NULL)
                                  ? -1
                                  : sqlite3_column_int(stmt, 2);
    std::string title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    const unsigned char* descText = sqlite3_column_text(stmt, 4);
    std::string description = descText ? reinterpret_cast<const char*>(descText) : "";
    TicketType type = stringToTicketType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
    TicketPriority priority = stringToPriority(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
    TicketStatus status = stringToStatus(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
    std::string createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    std::string updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    const unsigned char* resText = sqlite3_column_text(stmt, 10);
    std::string resolutionNotes = resText ? reinterpret_cast<const char*>(resText) : "";
    int feedbackRating = sqlite3_column_int(stmt, 11);
    const unsigned char* fbText = sqlite3_column_text(stmt, 12);
    std::string feedbackComment = fbText ? reinterpret_cast<const char*>(fbText) : "";

    return Ticket(id, employeeId, assignedEngineerId, title, description,
                  type, priority, status, createdAt, updatedAt, resolutionNotes,
                  feedbackRating, feedbackComment);
}

static const char* TICKET_SELECT_COLUMNS =
    "id, employee_id, assigned_engineer_id, title, description, ticket_type, priority, "
    "status, created_at, updated_at, resolution_notes, feedback_rating, feedback_comment";

Ticket DatabaseManager::getTicketById(int ticketId) {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS + " FROM tickets WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTicketById statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_int(stmt, 1, ticketId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        throw NotFoundException("Ticket with ID " + std::to_string(ticketId) + " not found.");
    }
    Ticket t = ticketFromRow(stmt);
    sqlite3_finalize(stmt);
    return t;
}

std::vector<Ticket> DatabaseManager::getAllTickets() {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS + " FROM tickets ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getAllTickets statement: ") + sqlite3_errmsg(db));
    }
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Ticket> DatabaseManager::getTicketsByEmployee(int employeeId) {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS + " FROM tickets WHERE employee_id = ? ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTicketsByEmployee statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_int(stmt, 1, employeeId);
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Ticket> DatabaseManager::getTicketsByEngineer(int engineerId) {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS + " FROM tickets WHERE assigned_engineer_id = ? ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTicketsByEngineer statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_int(stmt, 1, engineerId);
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Ticket> DatabaseManager::getTicketsByStatus(TicketStatus status) {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS + " FROM tickets WHERE status = ? ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTicketsByStatus statement: ") + sqlite3_errmsg(db));
    }
    std::string statusStr = statusToString(status);
    sqlite3_bind_text(stmt, 1, statusStr.c_str(), -1, SQLITE_TRANSIENT);
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Ticket> DatabaseManager::getActiveTicketsByEngineer(int engineerId) {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS +
        " FROM tickets WHERE assigned_engineer_id = ? AND " + ACTIVE_STATUS_CLAUSE + " ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getActiveTicketsByEngineer statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_int(stmt, 1, engineerId);
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Ticket> DatabaseManager::getResolvedTicketsByEngineer(int engineerId) {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS +
        " FROM tickets WHERE assigned_engineer_id = ? AND " + RESOLVED_STATUS_CLAUSE + " ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getResolvedTicketsByEngineer statement: ") + sqlite3_errmsg(db));
    }
    sqlite3_bind_int(stmt, 1, engineerId);
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<Ticket> DatabaseManager::getAllResolvedTickets() {
    std::string sql = std::string("SELECT ") + TICKET_SELECT_COLUMNS +
        " FROM tickets WHERE " + RESOLVED_STATUS_CLAUSE + " ORDER BY id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getAllResolvedTickets statement: ") + sqlite3_errmsg(db));
    }
    std::vector<Ticket> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(ticketFromRow(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

// ---------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------

std::map<std::string, int> DatabaseManager::getTicketCountByStatus() {
    const char* sql = "SELECT status, COUNT(*) FROM tickets GROUP BY status;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTicketCountByStatus statement: ") + sqlite3_errmsg(db));
    }
    std::map<std::string, int> counts;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int count = sqlite3_column_int(stmt, 1);
        counts[status] = count;
    }
    sqlite3_finalize(stmt);
    return counts;
}

std::map<std::string, int> DatabaseManager::getTicketCountByPriority() {
    const char* sql = "SELECT priority, COUNT(*) FROM tickets GROUP BY priority;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTicketCountByPriority statement: ") + sqlite3_errmsg(db));
    }
    std::map<std::string, int> counts;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string priority = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int count = sqlite3_column_int(stmt, 1);
        counts[priority] = count;
    }
    sqlite3_finalize(stmt);
    return counts;
}

double DatabaseManager::getAverageFeedbackRating() {
    const char* sql = "SELECT AVG(feedback_rating) FROM tickets WHERE feedback_rating > 0;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getAverageFeedbackRating statement: ") + sqlite3_errmsg(db));
    }
    double avg = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            avg = sqlite3_column_double(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return avg;
}

int DatabaseManager::getTotalTicketCount() {
    const char* sql = "SELECT COUNT(*) FROM tickets;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("Failed to prepare getTotalTicketCount statement: ") + sqlite3_errmsg(db));
    }
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

// ---------------------------------------------------------------------
// Atomic conditional writes
// ---------------------------------------------------------------------

// Assigns the ticket to engineerId only when:
//   - status = 'OPEN'
//   - assigned_engineer_id IS NULL
// Uses sqlite3_changes() to detect whether the row was actually modified.
// Throws LockConflictException when SQLite signals a write conflict.
bool DatabaseManager::atomicAssignTicket(int ticketId, int engineerId) {
    const char* sql =
        "UPDATE tickets "
        "SET assigned_engineer_id = ?, "
        "    status = ?, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ? "
        "  AND status = ? "
        "  AND assigned_engineer_id IS NULL;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("atomicAssignTicket prepare failed: ") + sqlite3_errmsg(db));
    }
    std::string assignedStatus = statusToString(TicketStatus::ASSIGNED);
    std::string openStatus     = statusToString(TicketStatus::OPEN);
    sqlite3_bind_int (stmt, 1, engineerId);
    sqlite3_bind_text(stmt, 2, assignedStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, ticketId);
    sqlite3_bind_text(stmt, 4, openStatus.c_str(),     -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    throwIfLockConflict(rc);
    if (rc != SQLITE_DONE) {
        throw DatabaseException(std::string("atomicAssignTicket step failed: ") + sqlite3_errmsg(db));
    }
    return sqlite3_changes(db) == 1;
}

// Moves the ticket to IN_PROGRESS only when:
//   - assigned_engineer_id = engineerId  (correct engineer)
//   - status = 'ASSIGNED'
bool DatabaseManager::atomicUpdateStatusInProgress(int ticketId, int engineerId) {
    const char* sql =
        "UPDATE tickets "
        "SET status = ?, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ? "
        "  AND assigned_engineer_id = ? "
        "  AND status = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("atomicUpdateStatusInProgress prepare failed: ") + sqlite3_errmsg(db));
    }
    std::string inProgressStatus = statusToString(TicketStatus::IN_PROGRESS);
    std::string assignedStatus   = statusToString(TicketStatus::ASSIGNED);
    sqlite3_bind_text(stmt, 1, inProgressStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, ticketId);
    sqlite3_bind_int (stmt, 3, engineerId);
    sqlite3_bind_text(stmt, 4, assignedStatus.c_str(),   -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    throwIfLockConflict(rc);
    if (rc != SQLITE_DONE) {
        throw DatabaseException(std::string("atomicUpdateStatusInProgress step failed: ") + sqlite3_errmsg(db));
    }
    return sqlite3_changes(db) == 1;
}

// Resolves the ticket only when:
//   - assigned_engineer_id = engineerId  (correct engineer)
//   - status IN ('ASSIGNED', 'IN_PROGRESS')  (not already resolved)
// Resolution text is never overwritten once written.
bool DatabaseManager::atomicResolveTicket(int ticketId, int engineerId,
                                          const std::string& resolutionNotes) {
    const char* sql =
        "UPDATE tickets "
        "SET status = ?, "
        "    resolution_notes = ?, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = ? "
        "  AND assigned_engineer_id = ? "
        "  AND status IN (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException(std::string("atomicResolveTicket prepare failed: ") + sqlite3_errmsg(db));
    }
    std::string resolvedStatus   = statusToString(TicketStatus::RESOLVED);
    std::string assignedStatus   = statusToString(TicketStatus::ASSIGNED);
    std::string inProgressStatus = statusToString(TicketStatus::IN_PROGRESS);
    sqlite3_bind_text(stmt, 1, resolvedStatus.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, resolutionNotes.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, ticketId);
    sqlite3_bind_int (stmt, 4, engineerId);
    sqlite3_bind_text(stmt, 5, assignedStatus.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, inProgressStatus.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    throwIfLockConflict(rc);
    if (rc != SQLITE_DONE) {
        throw DatabaseException(std::string("atomicResolveTicket step failed: ") + sqlite3_errmsg(db));
    }
    return sqlite3_changes(db) == 1;
}
