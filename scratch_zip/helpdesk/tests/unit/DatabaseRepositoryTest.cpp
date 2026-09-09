#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"

// -----------------------------------------------------------------------
// User CRUD
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, AddUserThenRetrieveById) {
    int id = db->addUser("Alice Employee", "alice", "pass123", "alice@company.com", UserRole::EMPLOYEE);
    UserRecord rec = db->getUserById(id);

    EXPECT_EQ(rec.name, "Alice Employee");
    EXPECT_EQ(rec.username, "alice");
    EXPECT_EQ(rec.email, "alice@company.com");
    EXPECT_EQ(rec.role, UserRole::EMPLOYEE);
}

TEST_F(TestDatabaseFixture, GetUserByIdThrowsNotFoundForMissingUser) {
    EXPECT_THROW(db->getUserById(9999), NotFoundException);
}

TEST_F(TestDatabaseFixture, GetAllUsersIncludesSeededAdmin) {
    auto users = db->getAllUsers();
    ASSERT_FALSE(users.empty());
    bool foundAdmin = false;
    for (const auto& u : users) {
        if (u.username == "admin") foundAdmin = true;
    }
    EXPECT_TRUE(foundAdmin);
}

TEST_F(TestDatabaseFixture, GetUsersByRoleFiltersCorrectly) {
    db->addUser("Bob Engineer", "bob", "pass123", "bob@company.com", UserRole::ENGINEER);
    db->addUser("Carol Engineer", "carol", "pass123", "carol@company.com", UserRole::ENGINEER);
    db->addUser("Dave Employee", "dave", "pass123", "dave@company.com", UserRole::EMPLOYEE);

    auto engineers = db->getUsersByRole(UserRole::ENGINEER);
    EXPECT_EQ(engineers.size(), 2u);
    for (const auto& e : engineers) {
        EXPECT_EQ(e.role, UserRole::ENGINEER);
    }
}

// -----------------------------------------------------------------------
// Ticket CRUD
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, AddTicketThenRetrieveById) {
    int empId = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    Ticket t(empId, "Wifi down", "No connectivity in building B", TicketType::NETWORK, TicketPriority::HIGH);
    int ticketId = db->addTicket(t);

    Ticket loaded = db->getTicketById(ticketId);
    EXPECT_EQ(loaded.getId(), ticketId);
    EXPECT_EQ(loaded.getTitle(), "Wifi down");
    EXPECT_EQ(loaded.getType(), TicketType::NETWORK);
    EXPECT_EQ(loaded.getPriority(), TicketPriority::HIGH);
    EXPECT_EQ(loaded.getStatus(), TicketStatus::OPEN);
    EXPECT_EQ(loaded.getEmployeeId(), empId);
}

TEST_F(TestDatabaseFixture, GetTicketByIdThrowsNotFoundForMissingTicket) {
    EXPECT_THROW(db->getTicketById(9999), NotFoundException);
}

TEST_F(TestDatabaseFixture, UpdateTicketPersistsChanges) {
    int empId = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob", "bob", "pass", "bob@company.com", UserRole::ENGINEER);
    Ticket t(empId, "Slow DB queries", "Reports take 10 minutes", TicketType::DATABASE, TicketPriority::CRITICAL);
    int ticketId = db->addTicket(t);

    Ticket loaded = db->getTicketById(ticketId);
    loaded.assignTo(engId);
    db->updateTicket(loaded);

    Ticket reloaded = db->getTicketById(ticketId);
    EXPECT_EQ(reloaded.getStatus(), TicketStatus::ASSIGNED);
    EXPECT_EQ(reloaded.getAssignedEngineerId(), engId);
}

TEST_F(TestDatabaseFixture, UpdateTicketRejectsUnpersistedTicket) {
    Ticket t(1, "Not saved yet", "desc", TicketType::OTHER, TicketPriority::LOW);
    EXPECT_THROW(db->updateTicket(t), ValidationException);
}

// -----------------------------------------------------------------------
// Filtering / queries
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, GetTicketsByEmployeeReturnsOnlyThatEmployeesTickets) {
    int alice = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    int bob = db->addUser("Bob", "bob", "pass", "bob@company.com", UserRole::EMPLOYEE);

    db->addTicket(Ticket(alice, "Alice ticket 1", "d", TicketType::OTHER, TicketPriority::LOW));
    db->addTicket(Ticket(alice, "Alice ticket 2", "d", TicketType::OTHER, TicketPriority::LOW));
    db->addTicket(Ticket(bob, "Bob ticket 1", "d", TicketType::OTHER, TicketPriority::LOW));

    auto aliceTickets = db->getTicketsByEmployee(alice);
    EXPECT_EQ(aliceTickets.size(), 2u);
    for (const auto& t : aliceTickets) {
        EXPECT_EQ(t.getEmployeeId(), alice);
    }
}

TEST_F(TestDatabaseFixture, GetTicketsByStatusFiltersCorrectly) {
    int emp = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    db->addTicket(Ticket(emp, "Open ticket", "d", TicketType::OTHER, TicketPriority::LOW));

    auto openTickets = db->getTicketsByStatus(TicketStatus::OPEN);
    EXPECT_EQ(openTickets.size(), 1u);
    EXPECT_EQ(openTickets[0].getStatus(), TicketStatus::OPEN);

    auto resolvedTickets = db->getTicketsByStatus(TicketStatus::RESOLVED);
    EXPECT_TRUE(resolvedTickets.empty());
}

// --- Resolved-ticket workflow separation, at the repository level ---
TEST_F(TestDatabaseFixture, ActiveEngineerQueryExcludesResolvedTickets) {
    int emp = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    int eng = db->addUser("Bob", "bob", "pass", "bob@company.com", UserRole::ENGINEER);

    Ticket t1(emp, "Ticket A", "d", TicketType::OTHER, TicketPriority::LOW);
    int id1 = db->addTicket(t1);
    Ticket loaded1 = db->getTicketById(id1);
    loaded1.assignTo(eng);
    db->updateTicket(loaded1);

    Ticket t2(emp, "Ticket B", "d", TicketType::OTHER, TicketPriority::LOW);
    int id2 = db->addTicket(t2);
    Ticket loaded2 = db->getTicketById(id2);
    loaded2.assignTo(eng);
    loaded2.resolve("Fixed");
    db->updateTicket(loaded2);

    auto active = db->getActiveTicketsByEngineer(eng);
    ASSERT_EQ(active.size(), 1u);
    EXPECT_EQ(active[0].getId(), id1);

    auto resolved = db->getResolvedTicketsByEngineer(eng);
    ASSERT_EQ(resolved.size(), 1u);
    EXPECT_EQ(resolved[0].getId(), id2);
}

TEST_F(TestDatabaseFixture, GetAllResolvedTicketsAggregatesAcrossEngineers) {
    int emp = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    int eng1 = db->addUser("Bob", "bob", "pass", "bob@company.com", UserRole::ENGINEER);
    int eng2 = db->addUser("Carol", "carol", "pass", "carol@company.com", UserRole::ENGINEER);

    for (int eng : {eng1, eng2}) {
        Ticket t(emp, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
        int id = db->addTicket(t);
        Ticket loaded = db->getTicketById(id);
        loaded.assignTo(eng);
        loaded.resolve("done");
        db->updateTicket(loaded);
    }

    EXPECT_EQ(db->getAllResolvedTickets().size(), 2u);
}

// -----------------------------------------------------------------------
// Reporting queries
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, TicketCountByStatusReflectsData) {
    int emp = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    db->addTicket(Ticket(emp, "T1", "d", TicketType::OTHER, TicketPriority::LOW));
    db->addTicket(Ticket(emp, "T2", "d", TicketType::OTHER, TicketPriority::LOW));

    auto counts = db->getTicketCountByStatus();
    EXPECT_EQ(counts["OPEN"], 2);
}

TEST_F(TestDatabaseFixture, TotalTicketCountIsAccurate) {
    int emp = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    EXPECT_EQ(db->getTotalTicketCount(), 0);
    db->addTicket(Ticket(emp, "T1", "d", TicketType::OTHER, TicketPriority::LOW));
    EXPECT_EQ(db->getTotalTicketCount(), 1);
}

// -----------------------------------------------------------------------
// Persistence across a fresh connection (same underlying file)
// -----------------------------------------------------------------------
TEST_F(TestDatabaseFixture, DataSurvivesReconnectingToSameDatabaseFile) {
    int emp = db->addUser("Alice", "alice", "pass", "alice@company.com", UserRole::EMPLOYEE);
    db->addTicket(Ticket(emp, "Persisted ticket", "d", TicketType::SOFTWARE, TicketPriority::MEDIUM));
    db->close();

    DatabaseManager reopened(dbPath);
    reopened.connect();
    // Not calling initializeSchema() again is intentional: this proves
    // the data is durably on disk, not just alive in the first
    // connection's in-memory state.
    auto tickets = reopened.getAllTickets();
    ASSERT_EQ(tickets.size(), 1u);
    EXPECT_EQ(tickets[0].getTitle(), "Persisted ticket");
    reopened.close();

    // Reassign db back to a valid open handle so TearDown's db->close() is safe.
    db = std::make_unique<DatabaseManager>(dbPath);
    db->connect();
}
