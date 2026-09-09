// =============================================================================
// ConcurrentWriteHardeningTest.cpp
// -----------------------------------------------------------------------------
// Focused tests for Task 2: SQLite Security and Concurrent-Write Hardening.
//
// Test coverage:
//   1. Apostrophes and SQL-looking text stored and retrieved safely.
//   2. No schema damage from malicious-looking input.
//   3. Two connections competing to assign one ticket -- exactly one wins.
//   4. Losing assignment does not overwrite the winner.
//   5. Wrong engineer cannot update (atomicUpdateStatusInProgress) a ticket.
//   6. Wrong engineer cannot resolve a ticket (atomicResolveTicket).
//   7. Repeated resolution changes zero rows the second time.
//   8. Original resolution text remains unchanged after repeated attempt.
//   9. Lock contention (SQLITE_BUSY simulation via BEGIN EXCLUSIVE) maps
//      to LockConflictException.
//  10. Connection remains usable after a rolled-back explicit transaction.
// =============================================================================

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <string>
#include <cstdio>
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"
#include "TestDatabaseFixture.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Creates a fresh, independent DatabaseManager for the given path (already
// schema-initialized by the primary `db` in the fixture).  The caller is
// responsible for calling connect() and close().
static std::unique_ptr<DatabaseManager> openSecondConnection(const std::string& path) {
    auto db2 = std::make_unique<DatabaseManager>(path);
    db2->connect();
    // Schema is already in place; do NOT call initializeSchema() again --
    // the second open must work with the existing tables.
    return db2;
}

// Inserts a minimal OPEN ticket via the primary fixture connection and
// returns its persisted ID.
static int seedOpenTicket(DatabaseManager& db, int empId) {
    Ticket t(empId, "Test Ticket", "Description", TicketType::OTHER, TicketPriority::MEDIUM);
    return db.addTicket(t);
}

// ---------------------------------------------------------------------------
// 1 & 2. Input-safety / SQL-injection hardening
// ---------------------------------------------------------------------------

// All user-facing strings that contain apostrophes or SQL metacharacters must
// be stored verbatim and retrieved without corruption.  A successful
// round-trip proves the prepared-statement binding sanitised the input.
TEST_F(TestDatabaseFixture, InputSafety_ApostropheInNameStoredSafely) {
    const std::string nameWithApostrophe = "O'Brien";
    int uid = db->addUser(nameWithApostrophe, "obrien", "pw",
                          "obrien@example.com", UserRole::EMPLOYEE);
    ASSERT_GT(uid, 0);
    UserRecord rec = db->getUserById(uid);
    EXPECT_EQ(rec.name, nameWithApostrophe);
}

TEST_F(TestDatabaseFixture, InputSafety_SqlInjectionInUsernameRejectedByValidation) {
    // Validation::validateUsernameOrThrow rejects non-alphanumeric-underscore chars,
    // so a classic injection payload fails at the validation layer before reaching SQL.
    EXPECT_THROW(
        db->addUser("Attacker", "'; DROP TABLE users; --", "pw",
                    "atk@example.com", UserRole::EMPLOYEE),
        ValidationException
    );
}

TEST_F(TestDatabaseFixture, InputSafety_SqlInjectionInTicketTitleStoredSafely) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    // Ticket titles are not username-validated; the title is a free-text field.
    // The prepared statement must store it verbatim and not interpret the SQL.
    const std::string injectionTitle = "'); DROP TABLE tickets; --";
    Ticket t(empId, injectionTitle, "desc", TicketType::OTHER, TicketPriority::LOW);
    int tid = db->addTicket(t);
    ASSERT_GT(tid, 0);
    Ticket loaded = db->getTicketById(tid);
    EXPECT_EQ(loaded.getTitle(), injectionTitle);
}

TEST_F(TestDatabaseFixture, InputSafety_SchemaIntactAfterMaliciousInput) {
    // After the injection attempt above, the schema must still be usable.
    int empId = db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::EMPLOYEE);
    Ticket t(empId, "Normal title", "Normal desc", TicketType::OTHER, TicketPriority::LOW);
    int tid = db->addTicket(t);
    EXPECT_GT(tid, 0);
    EXPECT_NO_THROW(db->getTicketById(tid));
}

TEST_F(TestDatabaseFixture, InputSafety_ApostropheInResolutionNotesStoredSafely) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob",   "bob",   "pw", "bob@company.com",   UserRole::ENGINEER);
    int tid = seedOpenTicket(*db, empId);

    ASSERT_TRUE(db->atomicAssignTicket(tid, engId));

    const std::string notesWithApostrophe = "It's fixed; O'Brien's patch applied.";
    ASSERT_TRUE(db->atomicResolveTicket(tid, engId, notesWithApostrophe));

    Ticket loaded = db->getTicketById(tid);
    EXPECT_EQ(loaded.getResolutionNotes(), notesWithApostrophe);
}

// ---------------------------------------------------------------------------
// 3 & 4. Two connections racing to assign one ticket
// ---------------------------------------------------------------------------

// Uses two independent DatabaseManager instances (each with their own sqlite3*
// handle and SQLITE_OPEN_FULLMUTEX).  One succeeds; the other gets zero rows
// changed.  No sleep is used -- SQLite's serialisation is deterministic for
// two sequential calls within the same process.
TEST_F(TestDatabaseFixture, ConcurrentAssign_ExactlyOneWins) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int eng1  = db->addUser("Eng1",  "eng1",  "pw", "eng1@company.com",  UserRole::ENGINEER);
    int eng2  = db->addUser("Eng2",  "eng2",  "pw", "eng2@company.com",  UserRole::ENGINEER);
    int tid   = seedOpenTicket(*db, empId);

    // Open a second, independent connection to the same file.
    auto db2 = openSecondConnection(dbPath);

    bool result1 = db->atomicAssignTicket(tid, eng1);
    bool result2 = db2->atomicAssignTicket(tid, eng2);

    // Exactly one must have changed a row.
    EXPECT_NE(result1, result2) << "Exactly one assignment must succeed";
    EXPECT_TRUE(result1 || result2) << "At least one must succeed";

    db2->close();
}

TEST_F(TestDatabaseFixture, ConcurrentAssign_LoserDoesNotOverwriteWinner) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int eng1  = db->addUser("Eng1",  "eng1",  "pw", "eng1@company.com",  UserRole::ENGINEER);
    int eng2  = db->addUser("Eng2",  "eng2",  "pw", "eng2@company.com",  UserRole::ENGINEER);
    int tid   = seedOpenTicket(*db, empId);

    auto db2 = openSecondConnection(dbPath);

    bool result1 = db->atomicAssignTicket(tid, eng1);
    bool result2 = db2->atomicAssignTicket(tid, eng2);
    (void)result2;  // read below via winner detection

    int winnerId = result1 ? eng1 : eng2;

    // Reload from the primary connection; the winning engineer ID must be intact.
    Ticket loaded = db->getTicketById(tid);
    EXPECT_EQ(loaded.getAssignedEngineerId(), winnerId)
        << "The losing attempt must not overwrite the winner's assignment";
    EXPECT_EQ(loaded.getStatus(), TicketStatus::ASSIGNED);

    db2->close();
}

// ---------------------------------------------------------------------------
// 5. Wrong engineer cannot move ticket to IN_PROGRESS
// ---------------------------------------------------------------------------

TEST_F(TestDatabaseFixture, AtomicUpdate_WrongEngineerCannotMoveToInProgress) {
    int empId    = db->addUser("Alice",       "alice",    "pw", "alice@co.com",  UserRole::EMPLOYEE);
    int rightEng = db->addUser("Right Eng",   "righteng", "pw", "right@co.com", UserRole::ENGINEER);
    int wrongEng = db->addUser("Wrong Eng",   "wrongeng", "pw", "wrong@co.com", UserRole::ENGINEER);
    int tid      = seedOpenTicket(*db, empId);

    ASSERT_TRUE(db->atomicAssignTicket(tid, rightEng));

    // Wrong engineer tries to move to IN_PROGRESS -- must change zero rows.
    bool changed = db->atomicUpdateStatusInProgress(tid, wrongEng);
    EXPECT_FALSE(changed) << "Wrong engineer must not be able to update status";

    // Status must remain ASSIGNED.
    EXPECT_EQ(db->getTicketById(tid).getStatus(), TicketStatus::ASSIGNED);
}

// ---------------------------------------------------------------------------
// 6. Wrong engineer cannot resolve a ticket
// ---------------------------------------------------------------------------

TEST_F(TestDatabaseFixture, AtomicResolve_WrongEngineerCannotResolve) {
    int empId    = db->addUser("Alice",     "alice",    "pw", "alice@co.com",  UserRole::EMPLOYEE);
    int rightEng = db->addUser("Right Eng", "righteng", "pw", "right@co.com", UserRole::ENGINEER);
    int wrongEng = db->addUser("Wrong Eng", "wrongeng", "pw", "wrong@co.com", UserRole::ENGINEER);
    int tid      = seedOpenTicket(*db, empId);

    ASSERT_TRUE(db->atomicAssignTicket(tid, rightEng));

    bool changed = db->atomicResolveTicket(tid, wrongEng, "Unauthorised resolution");
    EXPECT_FALSE(changed) << "Wrong engineer must not resolve this ticket";

    Ticket loaded = db->getTicketById(tid);
    EXPECT_NE(loaded.getStatus(), TicketStatus::RESOLVED);
    EXPECT_TRUE(loaded.getResolutionNotes().empty())
        << "Resolution notes must not be written by the wrong engineer";
}

// ---------------------------------------------------------------------------
// 7 & 8. Repeated resolution changes zero rows; original text is preserved
// ---------------------------------------------------------------------------

TEST_F(TestDatabaseFixture, AtomicResolve_RepeatedResolutionChangesZeroRows) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@co.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob",   "bob",   "pw", "bob@co.com",   UserRole::ENGINEER);
    int tid   = seedOpenTicket(*db, empId);

    ASSERT_TRUE(db->atomicAssignTicket(tid, engId));
    ASSERT_TRUE(db->atomicResolveTicket(tid, engId, "First resolution."));

    // Second attempt -- ticket is now RESOLVED, so status IN (ASSIGNED, IN_PROGRESS)
    // does not match and zero rows are changed.
    bool secondAttempt = db->atomicResolveTicket(tid, engId, "Second attempt to overwrite.");
    EXPECT_FALSE(secondAttempt) << "Second resolution must change zero rows";
}

TEST_F(TestDatabaseFixture, AtomicResolve_OriginalResolutionTextUnchanged) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@co.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob",   "bob",   "pw", "bob@co.com",   UserRole::ENGINEER);
    int tid   = seedOpenTicket(*db, empId);

    ASSERT_TRUE(db->atomicAssignTicket(tid, engId));

    const std::string original = "Original resolution text.";
    ASSERT_TRUE(db->atomicResolveTicket(tid, engId, original));

    // Attempt to overwrite with different text.
    db->atomicResolveTicket(tid, engId, "Overwrite attempt.");

    // The stored text must still match the original.
    Ticket loaded = db->getTicketById(tid);
    EXPECT_EQ(loaded.getResolutionNotes(), original)
        << "Original resolution notes must not be overwritten by a repeated call";
}

// ---------------------------------------------------------------------------
// 9. Lock contention maps to LockConflictException
//    Strategy: hold a BEGIN EXCLUSIVE transaction on a raw sqlite3 handle,
//    then attempt an atomic write through DatabaseManager on the same file.
//    SQLite will return SQLITE_BUSY; our helper must convert that.
// ---------------------------------------------------------------------------

TEST_F(TestDatabaseFixture, LockConflict_BusyMapsToLockConflictException) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@co.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob",   "bob",   "pw", "bob@co.com",   UserRole::ENGINEER);
    int tid   = seedOpenTicket(*db, empId);

    // Open a raw SQLite handle -- this simulates a competing external process
    // or a second connection that holds a write lock.
    sqlite3* raw = nullptr;
    ASSERT_EQ(SQLITE_OK,
        sqlite3_open_v2(dbPath.c_str(), &raw,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr));

    // Hold an exclusive transaction; this blocks all writes from other
    // connections.  We set a zero busy timeout so our DatabaseManager
    // connection returns SQLITE_BUSY immediately instead of waiting.
    sqlite3_busy_timeout(raw, 0);
    char* errMsg = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(raw, "BEGIN EXCLUSIVE;", nullptr, nullptr, &errMsg))
        << (errMsg ? errMsg : "unknown error");
    sqlite3_free(errMsg);

    // Our primary db handle also needs zero busy timeout so it bounces back
    // immediately with SQLITE_BUSY instead of spinning.
    sqlite3_busy_timeout(db->getRawHandle(), 0);

    EXPECT_THROW(db->atomicAssignTicket(tid, engId), LockConflictException);

    // Clean up: release the exclusive lock, then verify the connection is
    // still usable (requirement 10 -- connection usable after conflict).
    sqlite3_exec(raw, "ROLLBACK;", nullptr, nullptr, nullptr);
    sqlite3_close(raw);

    // Reset to a reasonable timeout and verify the connection still works.
    sqlite3_busy_timeout(db->getRawHandle(), 5000);
    EXPECT_NO_THROW({
        bool ok = db->atomicAssignTicket(tid, engId);
        EXPECT_TRUE(ok) << "Connection must be usable and assignment must succeed after conflict";
    });
}

// ---------------------------------------------------------------------------
// 10. Connection remains usable after explicit transaction rollback
// ---------------------------------------------------------------------------

TEST_F(TestDatabaseFixture, Transaction_ConnectionUsableAfterRollback) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@co.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob",   "bob",   "pw", "bob@co.com",   UserRole::ENGINEER);
    int tid   = seedOpenTicket(*db, empId);

    // Simulate a multi-statement transaction that is rolled back.
    EXPECT_NO_THROW(db->beginTransaction());
    // Do a write inside the transaction.
    ASSERT_TRUE(db->atomicAssignTicket(tid, engId));
    // Roll back -- the assignment must be undone.
    EXPECT_NO_THROW(db->rollback());

    // After rollback the ticket must still be OPEN.
    Ticket loaded = db->getTicketById(tid);
    EXPECT_EQ(loaded.getStatus(), TicketStatus::OPEN)
        << "Rollback must undo the assignment";
    EXPECT_EQ(loaded.getAssignedEngineerId(), -1)
        << "Rollback must undo the engineer assignment";

    // The connection must still be usable for further operations.
    EXPECT_NO_THROW({
        bool ok = db->atomicAssignTicket(tid, engId);
        EXPECT_TRUE(ok) << "Connection must work normally after rollback";
    });
    EXPECT_EQ(db->getTicketById(tid).getStatus(), TicketStatus::ASSIGNED);
}
