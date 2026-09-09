#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"

// Scenario: verify the "Assigned Tickets" query excludes RESOLVED/CLOSED
// tickets, and the "Resolved Tickets" query includes them -- the core
// fix for the "resolved tickets keep appearing as assigned" defect.
TEST_F(TestDatabaseFixture, ResolvedTicketFiltering_AssignedQueryExcludesResolved) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::ENGINEER);

    // Ticket 1: stays ASSIGNED
    Ticket t1(empId, "Still open issue", "d", TicketType::OTHER, TicketPriority::LOW);
    int id1 = db->addTicket(t1);
    Ticket loaded1 = db->getTicketById(id1);
    loaded1.assignTo(engId);
    db->updateTicket(loaded1);

    // Ticket 2: moves to IN_PROGRESS
    Ticket t2(empId, "Being worked on", "d", TicketType::OTHER, TicketPriority::LOW);
    int id2 = db->addTicket(t2);
    Ticket loaded2 = db->getTicketById(id2);
    loaded2.assignTo(engId);
    loaded2.updateStatus(TicketStatus::IN_PROGRESS);
    db->updateTicket(loaded2);

    // Ticket 3: fully RESOLVED -- must disappear from the active view
    Ticket t3(empId, "Already fixed", "d", TicketType::OTHER, TicketPriority::LOW);
    int id3 = db->addTicket(t3);
    Ticket loaded3 = db->getTicketById(id3);
    loaded3.assignTo(engId);
    loaded3.resolve("Fixed.");
    db->updateTicket(loaded3);

    auto activeTickets = db->getActiveTicketsByEngineer(engId);
    ASSERT_EQ(activeTickets.size(), 2u);
    for (const auto& t : activeTickets) {
        EXPECT_NE(t.getStatus(), TicketStatus::RESOLVED);
        EXPECT_NE(t.getStatus(), TicketStatus::CLOSED);
        EXPECT_NE(t.getId(), id3) << "resolved ticket must not appear in the assigned/active view";
    }
}

TEST_F(TestDatabaseFixture, ResolvedTicketFiltering_ResolvedQueryIncludesResolvedAndClosed) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::ENGINEER);

    Ticket resolvedT(empId, "Resolved one", "d", TicketType::OTHER, TicketPriority::LOW);
    int resolvedId = db->addTicket(resolvedT);
    Ticket loadedResolved = db->getTicketById(resolvedId);
    loadedResolved.assignTo(engId);
    loadedResolved.resolve("Done");
    db->updateTicket(loadedResolved);

    Ticket closedT(empId, "Closed one", "d", TicketType::OTHER, TicketPriority::LOW);
    int closedId = db->addTicket(closedT);
    Ticket loadedClosed = db->getTicketById(closedId);
    loadedClosed.assignTo(engId);
    loadedClosed.resolve("Done");
    loadedClosed.updateStatus(TicketStatus::CLOSED);
    db->updateTicket(loadedClosed);

    Ticket activeT(empId, "Still active", "d", TicketType::OTHER, TicketPriority::LOW);
    int activeId = db->addTicket(activeT);
    Ticket loadedActive = db->getTicketById(activeId);
    loadedActive.assignTo(engId);
    db->updateTicket(loadedActive);

    auto resolvedView = db->getResolvedTicketsByEngineer(engId);
    ASSERT_EQ(resolvedView.size(), 2u);

    std::vector<int> ids;
    for (const auto& t : resolvedView) ids.push_back(t.getId());
    EXPECT_NE(std::find(ids.begin(), ids.end(), resolvedId), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), closedId), ids.end());
    EXPECT_EQ(std::find(ids.begin(), ids.end(), activeId), ids.end())
        << "an active ticket must not appear in the resolved view";
}

TEST_F(TestDatabaseFixture, ResolvedTicketFiltering_EmptyResolvedViewWhenNothingResolvedYet) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Fresh ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int id = db->addTicket(t);
    Ticket loaded = db->getTicketById(id);
    loaded.assignTo(engId);
    db->updateTicket(loaded);

    EXPECT_TRUE(db->getResolvedTicketsByEngineer(engId).empty());
}
