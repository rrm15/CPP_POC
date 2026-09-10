#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"
#include "DateUtil.h"

// -----------------------------------------------------------------------
// Login notification integration tests
//
// Verifies one-time, persistent, role-specific login notifications:
//   - Admin  : new unassigned OPEN tickets
//   - Engineer: newly assigned tickets
//   - Employee: status changes to IN_PROGRESS / RESOLVED
// Confirms acknowledgment persists across DB reconnects.
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
static int addEmployee(DatabaseManager& db, const std::string& suffix) {
    return db.addUser("Emp " + suffix, "emp" + suffix, "pass", "emp" + suffix + "@x.com", UserRole::EMPLOYEE);
}

static int addEngineer(DatabaseManager& db, const std::string& suffix) {
    return db.addUser("Eng " + suffix, "eng" + suffix, "pass", "eng" + suffix + "@x.com", UserRole::ENGINEER);
}

// Shared old timestamp used to seed watermarks so newly created
// tickets/updates always have a strictly newer timestamp.
static const char* kOldWatermark = "2000-01-01 00:00:00";

// -----------------------------------------------------------------------
// Admin notifications
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, AdminNotification_NoOpenTickets_ReturnsEmpty) {
    UserRecord admin = db->authenticate("admin", "admin123");
    // Seed watermark at current time — no tickets created after, so nothing to report
    db->acknowledgeNotifications(admin.id);
    auto notes = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notes.empty());
}

TEST_F(TestDatabaseFixture, AdminNotification_NewOpenTicket_OneMessage) {
    UserRecord admin = db->authenticate("admin", "admin123");
    // Watermark in the far past — any new ticket is strictly newer
    db->recordInitialNotificationState(admin.id, kOldWatermark);

    int empId = addEmployee(*db, "a");
    Ticket t(empId, "Printer broken", "Out of toner", TicketType::HARDWARE, TicketPriority::LOW);
    db->addTicket(t);

    auto notes = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_TRUE(notes[0].find("awaiting assignment") != std::string::npos);
}

TEST_F(TestDatabaseFixture, AdminNotification_AcknowledgedPreventsRepeat) {
    UserRecord admin = db->authenticate("admin", "admin123");
    db->recordInitialNotificationState(admin.id, kOldWatermark);

    int empId = addEmployee(*db, "b");
    Ticket t(empId, "Server down", "Critical", TicketType::NETWORK, TicketPriority::CRITICAL);
    db->addTicket(t);

    // First login: see notification
    auto notes1 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_FALSE(notes1.empty());

    // Acknowledge at current time — tickets are now older than watermark
    db->acknowledgeNotifications(admin.id);

    // Second login: no new tickets since ack
    auto notes2 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notes2.empty());
}

TEST_F(TestDatabaseFixture, AdminNotification_PersistsAcrossReopen) {
    UserRecord admin = db->authenticate("admin", "admin123");
    db->recordInitialNotificationState(admin.id, kOldWatermark);

    int empId = addEmployee(*db, "c");
    Ticket t(empId, "VPN issue", "Cannot connect", TicketType::NETWORK, TicketPriority::MEDIUM);
    db->addTicket(t);
    db->acknowledgeNotifications(admin.id);

    // Simulate re-open (close and reopen same file)
    std::string path = dbPath;
    db->close();
    db.reset(new DatabaseManager(path));
    db->connect();
    db->initializeSchema();

    // No new tickets after ack — must still be empty
    auto notes = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notes.empty());
}

// -----------------------------------------------------------------------
// Engineer notifications
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, EngineerNotification_NewlyAssignedTicket_OneMessage) {
    int engId = addEngineer(*db, "1");
    // Watermark in the far past — assignment is strictly newer
    db->recordInitialNotificationState(engId, kOldWatermark);

    int empId = addEmployee(*db, "d");
    Ticket t(empId, "DB slow", "Query timeout", TicketType::DATABASE, TicketPriority::HIGH);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);

    auto notes = db->getPendingNotifications(engId, UserRole::ENGINEER);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_TRUE(notes[0].find("assigned") != std::string::npos);
}

TEST_F(TestDatabaseFixture, EngineerNotification_AcknowledgedPreventsRepeat) {
    int engId = addEngineer(*db, "2");
    db->recordInitialNotificationState(engId, kOldWatermark);

    int empId = addEmployee(*db, "e");
    Ticket t(empId, "Email issue", "Cannot receive", TicketType::APPLICATION_SUPPORT, TicketPriority::MEDIUM);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);

    auto notes1 = db->getPendingNotifications(engId, UserRole::ENGINEER);
    EXPECT_FALSE(notes1.empty());
    db->acknowledgeNotifications(engId);

    auto notes2 = db->getPendingNotifications(engId, UserRole::ENGINEER);
    EXPECT_TRUE(notes2.empty());
}

// -----------------------------------------------------------------------
// Employee notifications
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, EmployeeNotification_TicketMovedToInProgress_OneMessage) {
    int empId = addEmployee(*db, "f");
    int engId = addEngineer(*db, "3");
    // Watermark in the far past — status change is strictly newer
    db->recordInitialNotificationState(empId, kOldWatermark);

    Ticket t(empId, "Slow PC", "Takes 5 min to boot", TicketType::HARDWARE, TicketPriority::LOW);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);
    db->atomicUpdateStatusInProgress(tId, engId);

    auto notes = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_TRUE(notes[0].find("in progress") != std::string::npos ||
                notes[0].find("status update") != std::string::npos);
}

TEST_F(TestDatabaseFixture, EmployeeNotification_TicketResolved_OneMessage) {
    int empId = addEmployee(*db, "g");
    int engId = addEngineer(*db, "4");
    // Watermark in the far past — resolution is strictly newer
    db->recordInitialNotificationState(empId, kOldWatermark);

    Ticket t(empId, "No access", "Need printer access", TicketType::ACCESS_MANAGEMENT, TicketPriority::MEDIUM);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);
    db->atomicResolveTicket(tId, engId, "Access granted");

    auto notes = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_TRUE(notes[0].find("resolved") != std::string::npos ||
                notes[0].find("status update") != std::string::npos);
}

TEST_F(TestDatabaseFixture, EmployeeNotification_NoStateRecord_ReturnsAllActivity) {
    // When user has no watermark, getPendingNotifications should default to
    // epoch ("") and return anything (edge case: no crash)
    int empId = addEmployee(*db, "h");
    // Do NOT call recordInitialNotificationState - simulate new user with no state row
    auto notes = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    // No tickets for this employee yet - so notifications must be empty
    EXPECT_TRUE(notes.empty());
}

TEST_F(TestDatabaseFixture, NotificationState_RecordInitialIsIdempotent) {
    int engId = addEngineer(*db, "5");
    // Should not throw on duplicate call
    EXPECT_NO_THROW(db->recordInitialNotificationState(engId));
    EXPECT_NO_THROW(db->recordInitialNotificationState(engId));
}
