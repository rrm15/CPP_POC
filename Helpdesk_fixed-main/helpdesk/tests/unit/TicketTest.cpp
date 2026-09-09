#include <gtest/gtest.h>
#include "Ticket.h"
#include "Common.h"

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------
TEST(TicketTest, NewTicketStartsOpenAndUnassigned) {
    Ticket t(/*employeeId=*/7, "Printer jam", "Paper stuck in tray 2",
             TicketType::HARDWARE, TicketPriority::MEDIUM);

    EXPECT_EQ(t.getId(), -1); // not yet persisted
    EXPECT_EQ(t.getEmployeeId(), 7);
    EXPECT_EQ(t.getAssignedEngineerId(), -1);
    EXPECT_EQ(t.getStatus(), TicketStatus::OPEN);
    EXPECT_EQ(t.getType(), TicketType::HARDWARE);
    EXPECT_EQ(t.getPriority(), TicketPriority::MEDIUM);
    EXPECT_EQ(t.getTitle(), "Printer jam");
    EXPECT_EQ(t.getFeedbackRating(), 0);
}

TEST(TicketTest, EmptyTitleThrowsValidationException) {
    EXPECT_THROW(
        Ticket(1, "", "description", TicketType::OTHER, TicketPriority::LOW),
        ValidationException
    );
}

// -----------------------------------------------------------------------
// Ticket type assignment
// -----------------------------------------------------------------------
TEST(TicketTest, AllTicketTypesRoundTripThroughStringConversion) {
    const TicketType types[] = {
        TicketType::SOFTWARE, TicketType::HARDWARE, TicketType::INFOSEC,
        TicketType::NETWORK, TicketType::ACCESS_MANAGEMENT, TicketType::DATABASE,
        TicketType::APPLICATION_SUPPORT, TicketType::OTHER
    };
    for (TicketType type : types) {
        std::string s = ticketTypeToString(type);
        EXPECT_EQ(stringToTicketType(s), type) << "round-trip failed for " << s;
    }
}

TEST(TicketTest, UnknownTicketTypeStringThrows) {
    EXPECT_THROW(stringToTicketType("NOT_A_REAL_TYPE"), std::invalid_argument);
}

// -----------------------------------------------------------------------
// Status transitions
// -----------------------------------------------------------------------
TEST(TicketTest, AssignToMovesStatusToAssigned) {
    Ticket t(1, "VPN failure", "Cannot connect", TicketType::NETWORK, TicketPriority::HIGH);
    t.assignTo(42);
    EXPECT_EQ(t.getStatus(), TicketStatus::ASSIGNED);
    EXPECT_EQ(t.getAssignedEngineerId(), 42);
}

TEST(TicketTest, AssignToRejectsInvalidEngineerId) {
    Ticket t(1, "VPN failure", "Cannot connect", TicketType::NETWORK, TicketPriority::HIGH);
    EXPECT_THROW(t.assignTo(0), ValidationException);
    EXPECT_THROW(t.assignTo(-5), ValidationException);
}

TEST(TicketTest, UpdateStatusMovesThroughLifecycle) {
    Ticket t(1, "Slow laptop", "Boot takes 5 minutes", TicketType::HARDWARE, TicketPriority::LOW);
    t.assignTo(2);
    t.updateStatus(TicketStatus::IN_PROGRESS);
    EXPECT_EQ(t.getStatus(), TicketStatus::IN_PROGRESS);
}

TEST(TicketTest, CannotChangeStatusOfClosedTicket) {
    Ticket t(1, "Old ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    t.assignTo(2);
    t.updateStatus(TicketStatus::IN_PROGRESS);
    t.resolve("Fixed.");
    t.updateStatus(TicketStatus::CLOSED);
    EXPECT_THROW(t.updateStatus(TicketStatus::IN_PROGRESS), ValidationException);
}

TEST(TicketTest, CannotAssignAlreadyResolvedTicket) {
    Ticket t(1, "Old ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    t.assignTo(2);
    t.resolve("Done");
    EXPECT_THROW(t.assignTo(3), ValidationException);
}

// -----------------------------------------------------------------------
// Resolution logic
// -----------------------------------------------------------------------
TEST(TicketTest, ResolveRequiresAssignedEngineer) {
    Ticket t(1, "Unassigned ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    EXPECT_THROW(t.resolve("notes"), ValidationException);
}

TEST(TicketTest, ResolveRequiresNonEmptyNotes) {
    Ticket t(1, "Ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    t.assignTo(9);
    EXPECT_THROW(t.resolve(""), ValidationException);
}

TEST(TicketTest, ResolveSetsStatusAndNotes) {
    Ticket t(1, "Ticket", "desc", TicketType::SOFTWARE, TicketPriority::CRITICAL);
    t.assignTo(9);
    t.resolve("Reinstalled the application.");
    EXPECT_EQ(t.getStatus(), TicketStatus::RESOLVED);
    EXPECT_EQ(t.getResolutionNotes(), "Reinstalled the application.");
}

// -----------------------------------------------------------------------
// Assignment retention (engineer assignment survives status changes and
// is preserved through the resolve step, per the resolved-ticket
// workflow requirement).
// -----------------------------------------------------------------------
TEST(TicketTest, EngineerAssignmentIsRetainedThroughResolution) {
    Ticket t(1, "Ticket", "desc", TicketType::DATABASE, TicketPriority::HIGH);
    t.assignTo(55);
    t.updateStatus(TicketStatus::IN_PROGRESS);
    t.resolve("Restored from backup.");
    EXPECT_EQ(t.getAssignedEngineerId(), 55) << "engineer assignment must survive resolution";
    EXPECT_EQ(t.getType(), TicketType::DATABASE) << "ticket type must survive resolution";
}

// -----------------------------------------------------------------------
// Feedback
// -----------------------------------------------------------------------
TEST(TicketTest, FeedbackRequiresResolvedOrClosedStatus) {
    Ticket t(1, "Ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    EXPECT_THROW(t.addFeedback(5, "Great"), ValidationException);
}

TEST(TicketTest, FeedbackRejectsOutOfRangeRating) {
    Ticket t(1, "Ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    t.assignTo(1);
    t.resolve("done");
    EXPECT_THROW(t.addFeedback(0, "bad"), ValidationException);
    EXPECT_THROW(t.addFeedback(6, "bad"), ValidationException);
}

TEST(TicketTest, FeedbackAcceptedOnResolvedTicket) {
    Ticket t(1, "Ticket", "desc", TicketType::OTHER, TicketPriority::LOW);
    t.assignTo(1);
    t.resolve("done");
    t.addFeedback(5, "Excellent support");
    EXPECT_EQ(t.getFeedbackRating(), 5);
    EXPECT_EQ(t.getFeedbackComment(), "Excellent support");
}
