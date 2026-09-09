#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"

// Scenario: Create -> Save -> Reload -> Verify
TEST_F(TestDatabaseFixture, TicketPersistenceScenario_CreateSaveReloadVerify) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);

    Ticket original(empId, "Database connection pool exhausted",
                     "Application errors under load", TicketType::DATABASE, TicketPriority::CRITICAL);
    int ticketId = db->addTicket(original);

    Ticket reloaded = db->getTicketById(ticketId);

    EXPECT_EQ(reloaded.getTitle(), original.getTitle());
    EXPECT_EQ(reloaded.getDescription(), original.getDescription());
    EXPECT_EQ(reloaded.getType(), original.getType());
    EXPECT_EQ(reloaded.getPriority(), original.getPriority());
    EXPECT_EQ(reloaded.getStatus(), TicketStatus::OPEN);
    EXPECT_EQ(reloaded.getEmployeeId(), empId);
}

// Scenario: Save Type -> Reload -> Verify, across every supported
// TicketType value (ticket-type matrix at the persistence layer).
class TicketTypePersistenceTest : public TestDatabaseFixture,
                                    public ::testing::WithParamInterface<TicketType> {};

TEST_P(TicketTypePersistenceTest, TicketTypeSurvivesSaveAndReload) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    TicketType type = GetParam();

    Ticket t(empId, "Type persistence check", "d", type, TicketPriority::MEDIUM);
    int ticketId = db->addTicket(t);

    Ticket reloaded = db->getTicketById(ticketId);
    EXPECT_EQ(reloaded.getType(), type)
        << "ticket type " << ticketTypeToString(type) << " did not survive persistence";
}

INSTANTIATE_TEST_SUITE_P(
    AllTicketTypes,
    TicketTypePersistenceTest,
    ::testing::Values(
        TicketType::SOFTWARE, TicketType::HARDWARE, TicketType::INFOSEC,
        TicketType::NETWORK, TicketType::ACCESS_MANAGEMENT, TicketType::DATABASE,
        TicketType::APPLICATION_SUPPORT, TicketType::OTHER
    )
);

// Reload via a second, independent DatabaseManager instance pointed at
// the same file -- proves the ticket type was actually written to disk
// rather than merely cached in the first connection's process memory.
TEST_F(TestDatabaseFixture, TicketTypePersistenceScenario_SurvivesReconnection) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    db->addTicket(Ticket(empId, "Access request", "New hire needs VPN access",
                          TicketType::ACCESS_MANAGEMENT, TicketPriority::MEDIUM));
    db->close();

    DatabaseManager reopened(dbPath);
    reopened.connect();
    auto tickets = reopened.getAllTickets();
    ASSERT_EQ(tickets.size(), 1u);
    EXPECT_EQ(tickets[0].getType(), TicketType::ACCESS_MANAGEMENT);
    reopened.close();

    db = std::make_unique<DatabaseManager>(dbPath);
    db->connect();
}
