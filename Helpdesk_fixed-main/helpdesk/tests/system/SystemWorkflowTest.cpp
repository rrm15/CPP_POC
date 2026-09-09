#include <gtest/gtest.h>
#include <vector>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"

// Full system-level workflow exercising the whole stack through the
// public DatabaseManager/Ticket API (the same API the console menus in
// User.cpp/main.cpp drive): Register -> Authenticate -> Create Ticket ->
// Assign Engineer -> Mark In Progress -> Resolve -> Verify Assigned View
// -> Verify Resolved View -> Restart "Service" -> Verify Persistence.
TEST_F(TestDatabaseFixture, SystemWorkflow_CompleteTicketLifecycleWithRestart) {
    // Register User (Employee) + provision an Engineer account, as an
    // Admin would via Admin::manageUsersFlow.
    int employeeId = db->addUser("Grace Employee", "grace", "pw123",
                                  "grace@company.com", UserRole::EMPLOYEE);
    int engineerId = db->addUser("Henry Engineer", "henry", "pw123",
                                  "henry@company.com", UserRole::ENGINEER);

    // Authenticate
    UserRecord employeeLogin = db->authenticate("grace", "pw123");
    ASSERT_EQ(employeeLogin.id, employeeId);
    UserRecord engineerLogin = db->authenticate("henry", "pw123");
    ASSERT_EQ(engineerLogin.id, engineerId);

    // Create Ticket
    Ticket newTicket(employeeId, "Cannot access shared drive",
                      "Permission denied error on \\\\fileserver\\shared",
                      TicketType::ACCESS_MANAGEMENT, TicketPriority::HIGH);
    int ticketId = db->addTicket(newTicket);

    // Assign Engineer
    Ticket assigned = db->getTicketById(ticketId);
    assigned.assignTo(engineerId);
    db->updateTicket(assigned);

    // Mark In Progress
    Ticket inProgress = db->getTicketById(ticketId);
    inProgress.updateStatus(TicketStatus::IN_PROGRESS);
    db->updateTicket(inProgress);

    // Verify Assigned View (should show it while still in progress)
    {
        auto activeTickets = db->getActiveTicketsByEngineer(engineerId);
        ASSERT_EQ(activeTickets.size(), 1u);
        EXPECT_EQ(activeTickets[0].getId(), ticketId);
        EXPECT_EQ(activeTickets[0].getStatus(), TicketStatus::IN_PROGRESS);
    }

    // Resolve Ticket
    Ticket toResolve = db->getTicketById(ticketId);
    toResolve.resolve("Re-added the user to the shared-drive access group.");
    db->updateTicket(toResolve);

    // Verify Assigned View no longer shows it
    EXPECT_TRUE(db->getActiveTicketsByEngineer(engineerId).empty());

    // Verify Resolved View shows it
    {
        auto resolvedTickets = db->getResolvedTicketsByEngineer(engineerId);
        ASSERT_EQ(resolvedTickets.size(), 1u);
        EXPECT_EQ(resolvedTickets[0].getId(), ticketId);
        EXPECT_EQ(resolvedTickets[0].getStatus(), TicketStatus::RESOLVED);
    }

    // Restart Services -- simulated by closing this connection and
    // opening a brand-new DatabaseManager against the same file, exactly
    // as would happen if the console application were quit and relaunched.
    db->close();
    {
        DatabaseManager restarted(dbPath);
        restarted.connect();

        // Verify Persistence: everything survives the "restart".
        UserRecord reloadedEmployee = restarted.getUserById(employeeId);
        EXPECT_EQ(reloadedEmployee.username, "grace");

        Ticket reloadedTicket = restarted.getTicketById(ticketId);
        EXPECT_EQ(reloadedTicket.getStatus(), TicketStatus::RESOLVED);
        EXPECT_EQ(reloadedTicket.getAssignedEngineerId(), engineerId);
        EXPECT_EQ(reloadedTicket.getType(), TicketType::ACCESS_MANAGEMENT);
        EXPECT_FALSE(reloadedTicket.getResolutionNotes().empty());

        auto resolvedAfterRestart = restarted.getResolvedTicketsByEngineer(engineerId);
        EXPECT_EQ(resolvedAfterRestart.size(), 1u);

        restarted.close();
    }

    // Leave the fixture's db handle valid for TearDown().
    db = std::make_unique<DatabaseManager>(dbPath);
    db->connect();
}

// Ticket-type matrix at the system level: one ticket of every supported
// type is created within a single realistic session and all must persist
// correctly and independently.
TEST_F(TestDatabaseFixture, SystemWorkflow_TicketTypeMatrixAcrossFullSession) {
    int employeeId = db->addUser("Ivy Employee", "ivy", "pw", "ivy@company.com", UserRole::EMPLOYEE);

    const TicketType allTypes[] = {
        TicketType::SOFTWARE, TicketType::HARDWARE, TicketType::INFOSEC,
        TicketType::NETWORK, TicketType::ACCESS_MANAGEMENT, TicketType::DATABASE,
        TicketType::APPLICATION_SUPPORT, TicketType::OTHER
    };

    std::vector<int> createdIds;
    for (TicketType type : allTypes) {
        Ticket t(employeeId, "Ticket for " + ticketTypeToString(type),
                 "auto-generated", type, TicketPriority::MEDIUM);
        createdIds.push_back(db->addTicket(t));
    }

    ASSERT_EQ(createdIds.size(), 8u);
    for (size_t i = 0; i < createdIds.size(); ++i) {
        Ticket reloaded = db->getTicketById(createdIds[i]);
        EXPECT_EQ(reloaded.getType(), allTypes[i]);
    }

    EXPECT_EQ(db->getTotalTicketCount(), 8);
}
