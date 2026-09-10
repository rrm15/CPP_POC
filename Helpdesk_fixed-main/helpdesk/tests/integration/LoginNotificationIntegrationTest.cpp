#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"
#include "DateUtil.h"

// -----------------------------------------------------------------------
// Login notification integration tests
//
// Verifies production-path local-time notification delivery and
// millisecond-monotonic acknowledgement semantics:
//   - Admin  : new unassigned OPEN tickets
//   - Engineer: newly assigned tickets
//   - Employee: status changes (IN_PROGRESS, RESOLVED)
// -----------------------------------------------------------------------

static int addEmployee(DatabaseManager& db, const std::string& suffix) {
    return db.addUser("Emp " + suffix, "emp_" + suffix, "pass123", "emp_" + suffix + "@test.com", UserRole::EMPLOYEE);
}

static int addEngineer(DatabaseManager& db, const std::string& suffix) {
    return db.addUser("Eng " + suffix, "eng_" + suffix, "pass123", "eng_" + suffix + "@test.com", UserRole::ENGINEER);
}

TEST_F(TestDatabaseFixture, AdminSeesNewlyCreatedUnassignedTicketOnce) {
    UserRecord admin = db->authenticate("admin", "admin123");
    db->acknowledgeNotifications(admin.id);

    int empId = addEmployee(*db, "admin_test1");
    Ticket t(empId, "Printer Broken", "Out of paper", TicketType::HARDWARE, TicketPriority::LOW);
    db->addTicket(t);

    auto notes = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0], "You have 1 new ticket awaiting assignment.");

    db->acknowledgeNotifications(admin.id);
    auto notesAfterAck = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notesAfterAck.empty());
}

TEST_F(TestDatabaseFixture, EngineerSeesNewlyAssignedTicketOnce) {
    int engId = addEngineer(*db, "assign1");
    db->acknowledgeNotifications(engId);

    int empId = addEmployee(*db, "assign1_emp");
    Ticket t(empId, "Database Timeout", "Query stalls", TicketType::DATABASE, TicketPriority::HIGH);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);

    auto notes = db->getPendingNotifications(engId, UserRole::ENGINEER);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0], "You have 1 newly assigned ticket.");

    db->acknowledgeNotifications(engId);
    auto notesAfterAck = db->getPendingNotifications(engId, UserRole::ENGINEER);
    EXPECT_TRUE(notesAfterAck.empty());
}

TEST_F(TestDatabaseFixture, EmployeeSeesInProgressUpdateOnce) {
    int empId = addEmployee(*db, "inp_emp");
    int engId = addEngineer(*db, "inp_eng");
    db->acknowledgeNotifications(empId);

    Ticket t(empId, "Network Down", "No wifi in 2nd floor", TicketType::NETWORK, TicketPriority::CRITICAL);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);
    db->atomicUpdateStatusInProgress(tId, engId);

    auto notes = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0], "Ticket #" + std::to_string(tId) + " is now in progress.");

    db->acknowledgeNotifications(empId);
    auto notesAfterAck = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    EXPECT_TRUE(notesAfterAck.empty());
}

TEST_F(TestDatabaseFixture, EmployeeSeesResolvedUpdateOnce) {
    int empId = addEmployee(*db, "res_emp");
    int engId = addEngineer(*db, "res_eng");
    db->acknowledgeNotifications(empId);

    Ticket t(empId, "Password Reset", "Forgot employee password", TicketType::ACCESS_MANAGEMENT, TicketPriority::MEDIUM);
    int tId = db->addTicket(t);
    db->atomicAssignTicket(tId, engId);
    db->atomicUpdateStatusInProgress(tId, engId);
    db->acknowledgeNotifications(empId);

    db->atomicResolveTicket(tId, engId, "Password reset link sent to registered email");

    auto notes = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0], "Ticket #" + std::to_string(tId) + " has been resolved.");

    db->acknowledgeNotifications(empId);
    auto notesAfterAck = db->getPendingNotifications(empId, UserRole::EMPLOYEE);
    EXPECT_TRUE(notesAfterAck.empty());
}

TEST_F(TestDatabaseFixture, AcknowledgedNotificationsDoNotReappear) {
    UserRecord admin = db->authenticate("admin", "admin123");
    db->acknowledgeNotifications(admin.id);

    int empId = addEmployee(*db, "ack_repeat");
    Ticket t(empId, "Bug in Portal", "Button misaligned", TicketType::SOFTWARE, TicketPriority::LOW);
    db->addTicket(t);

    auto notes1 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_FALSE(notes1.empty());

    db->acknowledgeNotifications(admin.id);

    auto notes2 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notes2.empty());

    auto notes3 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notes3.empty());
}

TEST_F(TestDatabaseFixture, LaterEventAppearsAfterAcknowledgement) {
    UserRecord admin = db->authenticate("admin", "admin123");
    db->acknowledgeNotifications(admin.id);

    int empId = addEmployee(*db, "later_evt");
    Ticket t1(empId, "Issue 1", "First problem", TicketType::OTHER, TicketPriority::LOW);
    db->addTicket(t1);

    auto notes1 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_EQ(notes1.size(), 1u);
    db->acknowledgeNotifications(admin.id);

    // Later event after ack
    Ticket t2(empId, "Issue 2", "Second problem", TicketType::OTHER, TicketPriority::HIGH);
    db->addTicket(t2);

    auto notes2 = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    ASSERT_EQ(notes2.size(), 1u);
    EXPECT_EQ(notes2[0], "You have 1 new ticket awaiting assignment.");
}

TEST_F(TestDatabaseFixture, EventsCreatedImmediatelyAfterAcknowledgementAreNotMissed) {
    UserRecord admin = db->authenticate("admin", "admin123");
    int empId = addEmployee(*db, "imm_emp");

    // Immediately back-to-back acknowledgment and event creation
    db->acknowledgeNotifications(admin.id);

    Ticket t(empId, "Immediate Event", "Created immediately after ack", TicketType::HARDWARE, TicketPriority::MEDIUM);
    db->addTicket(t);

    auto notes = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    ASSERT_EQ(notes.size(), 1u);
    EXPECT_EQ(notes[0], "You have 1 new ticket awaiting assignment.");
}

TEST_F(TestDatabaseFixture, NotificationStateSurvivesDatabaseReopen) {
    UserRecord admin = db->authenticate("admin", "admin123");
    int empId = addEmployee(*db, "reopen_emp");

    Ticket t(empId, "Reopen Test", "Testing state persistence", TicketType::SOFTWARE, TicketPriority::LOW);
    db->addTicket(t);
    db->acknowledgeNotifications(admin.id);

    // Simulate DB close & reopen
    std::string path = dbPath;
    db->close();
    db.reset(new DatabaseManager(path));
    db->connect();
    db->initializeSchema();

    auto notes = db->getPendingNotifications(admin.id, UserRole::ADMIN);
    EXPECT_TRUE(notes.empty());
}

TEST_F(TestDatabaseFixture, OneUserCannotAcknowledgeAnotherUsersEvents) {
    int eng1 = addEngineer(*db, "userA");
    int eng2 = addEngineer(*db, "userB");
    int empId = addEmployee(*db, "userAB_emp");

    Ticket t1(empId, "Ticket for Eng 1", "Work for Eng 1", TicketType::NETWORK, TicketPriority::HIGH);
    int tId1 = db->addTicket(t1);
    db->atomicAssignTicket(tId1, eng1);

    Ticket t2(empId, "Ticket for Eng 2", "Work for Eng 2", TicketType::SOFTWARE, TicketPriority::HIGH);
    int tId2 = db->addTicket(t2);
    db->atomicAssignTicket(tId2, eng2);

    // Eng 1 acknowledges Eng 1's notifications
    db->acknowledgeNotifications(eng1);

    // Eng 1 has 0 pending notifications
    EXPECT_TRUE(db->getPendingNotifications(eng1, UserRole::ENGINEER).empty());

    // Eng 2 STILL has Eng 2's notification pending
    auto notesEng2 = db->getPendingNotifications(eng2, UserRole::ENGINEER);
    ASSERT_EQ(notesEng2.size(), 1u);
    EXPECT_EQ(notesEng2[0], "You have 1 newly assigned ticket.");
}

TEST_F(TestDatabaseFixture, StoredTimestampsUseLocalFractionalFormat) {
    int empId = addEmployee(*db, "fmt_emp");
    Ticket t(empId, "Format Check", "Verifying timestamp format", TicketType::OTHER, TicketPriority::LOW);
    int tId = db->addTicket(t);

    Ticket loaded = db->getTicketById(tId);
    std::string createdAt = loaded.getCreatedAt();
    std::string updatedAt = loaded.getUpdatedAt();

    // Verify format YYYY-MM-DD HH:MM:SS.SSS (contains a dot followed by 3 digits)
    EXPECT_NE(createdAt.find('.'), std::string::npos);
    EXPECT_NE(updatedAt.find('.'), std::string::npos);
    EXPECT_EQ(createdAt.length(), 23u); // 19 + 1 + 3 = 23 chars
    EXPECT_EQ(updatedAt.length(), 23u);
}
