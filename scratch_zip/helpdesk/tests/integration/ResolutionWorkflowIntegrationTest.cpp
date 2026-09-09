#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"

// Scenario: Assign -> In Progress -> Resolve -> Reload -> Verify
TEST_F(TestDatabaseFixture, ResolutionWorkflowScenario_FullLifecycle) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Application crashes on startup", "Stack trace attached",
             TicketType::APPLICATION_SUPPORT, TicketPriority::HIGH);
    int ticketId = db->addTicket(t);

    // Assign
    Ticket step1 = db->getTicketById(ticketId);
    step1.assignTo(engId);
    db->updateTicket(step1);
    EXPECT_EQ(db->getTicketById(ticketId).getStatus(), TicketStatus::ASSIGNED);

    // In Progress
    Ticket step2 = db->getTicketById(ticketId);
    step2.updateStatus(TicketStatus::IN_PROGRESS);
    db->updateTicket(step2);
    EXPECT_EQ(db->getTicketById(ticketId).getStatus(), TicketStatus::IN_PROGRESS);

    // Resolve
    Ticket step3 = db->getTicketById(ticketId);
    step3.resolve("Patched a null pointer dereference in the startup sequence.");
    db->updateTicket(step3);

    // Reload -> Verify
    Ticket final = db->getTicketById(ticketId);
    EXPECT_EQ(final.getStatus(), TicketStatus::RESOLVED);
    EXPECT_EQ(final.getAssignedEngineerId(), engId) << "engineer assignment must be preserved";
    EXPECT_EQ(final.getType(), TicketType::APPLICATION_SUPPORT) << "ticket type must be preserved";
    EXPECT_FALSE(final.getResolutionNotes().empty());
}

TEST_F(TestDatabaseFixture, ResolutionWorkflowScenario_FeedbackAfterResolution) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    int ticketId = db->addTicket(t);

    Ticket loaded = db->getTicketById(ticketId);
    loaded.assignTo(engId);
    loaded.resolve("Resolved.");
    db->updateTicket(loaded);

    Ticket toRate = db->getTicketById(ticketId);
    toRate.addFeedback(4, "Good, but took a while.");
    db->updateTicket(toRate);

    Ticket final = db->getTicketById(ticketId);
    EXPECT_EQ(final.getFeedbackRating(), 4);
    EXPECT_EQ(final.getFeedbackComment(), "Good, but took a while.");
    EXPECT_DOUBLE_EQ(db->getAverageFeedbackRating(), 4.0);
}
