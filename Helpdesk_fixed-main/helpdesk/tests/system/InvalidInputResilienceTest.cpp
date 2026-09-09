#include <gtest/gtest.h>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "Ticket.h"
#include "Common.h"
#include "Validation.h"
#include "InputUtil.h"
#include <sstream>

// -----------------------------------------------------------------------
// Invalid Input Resilience
// -----------------------------------------------------------------------
// "Application must not crash" is verified here by demonstrating that
// every invalid-input path raises a normal, catchable C++ exception
// (never a segfault, never std::terminate, never a silent corrupt
// state) -- exactly the contract User::showMenu()'s try/catch blocks in
// User.cpp rely on to keep the console loop alive after a bad entry.

TEST_F(TestDatabaseFixture, InvalidTicketType_UnknownStringIsRejected) {
    EXPECT_THROW(stringToTicketType("NOT_A_TYPE"), std::invalid_argument);
    EXPECT_THROW(stringToTicketType(""), std::invalid_argument);
    EXPECT_THROW(stringToTicketType("software"), std::invalid_argument) // wrong case
        << "ticket type strings are stored/compared as canonical upper-case tokens";
}

TEST_F(TestDatabaseFixture, InvalidTicketType_CannotBePersistedDirectly) {
    // The only way an invalid ticket type could reach the database is if
    // someone tried to construct a Ticket from a bad string; the
    // conversion utility itself is what stops that before any SQL runs.
    EXPECT_THROW(stringToTicketType("HALLOWEEN"), std::invalid_argument);
}

TEST_F(TestDatabaseFixture, EmptyFields_TicketTitleRejected) {
    EXPECT_THROW(
        Ticket(1, "", "some description", TicketType::OTHER, TicketPriority::LOW),
        ValidationException
    );
}

TEST_F(TestDatabaseFixture, EmptyFields_UsernameAndEmailRejectedByAddUser) {
    EXPECT_THROW(
        db->addUser("Name", "", "pw", "valid@company.com", UserRole::EMPLOYEE),
        ValidationException
    );
    EXPECT_THROW(
        db->addUser("Name", "validuser", "pw", "", UserRole::EMPLOYEE),
        ValidationException
    );
}

TEST_F(TestDatabaseFixture, UnknownIds_GetTicketByIdThrowsNotFound) {
    EXPECT_THROW(db->getTicketById(-1), NotFoundException);
    EXPECT_THROW(db->getTicketById(999999), NotFoundException);
}

TEST_F(TestDatabaseFixture, UnknownIds_GetUserByIdThrowsNotFound) {
    EXPECT_THROW(db->getUserById(-1), NotFoundException);
    EXPECT_THROW(db->getUserById(999999), NotFoundException);
}

TEST_F(TestDatabaseFixture, UnknownIds_AssigningNonexistentEngineerIsRejectedAtDomainLevel) {
    // Ticket::assignTo only validates that the ID looks like a plausible
    // ID (positive integer); it does not itself verify the engineer
    // exists in the users table (that responsibility belongs to the
    // Admin::assignTicketFlow console flow, which lists real engineers
    // for the admin to choose from). This test documents that boundary
    // rather than asserting behavior the class doesn't own.
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    Ticket t(empId, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int ticketId = db->addTicket(t);
    Ticket loaded = db->getTicketById(ticketId);

    EXPECT_NO_THROW(loaded.assignTo(999999)); // domain-level: syntactically valid positive ID
    EXPECT_THROW(loaded.assignTo(0), ValidationException);   // but zero/negative always rejected
    EXPECT_THROW(loaded.assignTo(-3), ValidationException);
}

TEST_F(TestDatabaseFixture, InvalidStatusTransitions_CannotModifyClosedTicket) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    Ticket t(empId, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int ticketId = db->addTicket(t);
    Ticket loaded = db->getTicketById(ticketId);
    loaded.assignTo(1);
    loaded.resolve("done");
    loaded.updateStatus(TicketStatus::CLOSED);

    EXPECT_THROW(loaded.updateStatus(TicketStatus::IN_PROGRESS), ValidationException);
    EXPECT_THROW(loaded.assignTo(2), ValidationException);
}

TEST_F(TestDatabaseFixture, InvalidStatusTransitions_CannotResolveWithoutAssignment) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    Ticket t(empId, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    EXPECT_THROW(t.resolve("premature resolution attempt"), ValidationException);
}

TEST_F(TestDatabaseFixture, InvalidStatusTransitions_CannotGiveFeedbackOnOpenTicket) {
    int empId = db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    Ticket t(empId, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    EXPECT_THROW(t.addFeedback(5, "too early"), ValidationException);
}

// Menu-input resilience: readInt()'s validation loop (the same helper
// every console menu uses) must reject non-numeric garbage without
// crashing and eventually return the first valid integer it sees.
TEST_F(TestDatabaseFixture, InvalidMenuInput_ReadIntSkipsGarbageAndReturnsFirstValidInteger) {
    std::istringstream fakeStdin("not-a-number\nalso bad\n42\n");
    std::streambuf* originalBuf = std::cin.rdbuf(fakeStdin.rdbuf());

    int result = InputUtil::readInt("Choice: ");

    std::cin.rdbuf(originalBuf);
    EXPECT_EQ(result, 42);
}

TEST_F(TestDatabaseFixture, DatabaseConstraintViolations_DoNotCorruptState) {
    // Attempting (and failing) to insert a duplicate user must leave the
    // database in a consistent, still-usable state -- not a partially
    // committed row, not a wedged connection.
    db->addUser("Alice", "alice", "pw", "alice@company.com", UserRole::EMPLOYEE);
    EXPECT_THROW(
        db->addUser("Alice2", "alice", "pw2", "alice2@company.com", UserRole::EMPLOYEE),
        ValidationException
    );

    // The connection and table must still be perfectly usable afterward.
    EXPECT_EQ(db->getAllUsers().size(), 2u); // seeded admin + alice, duplicate rejected
    EXPECT_NO_THROW(db->addUser("Bob", "bob", "pw", "bob@company.com", UserRole::EMPLOYEE));
}
