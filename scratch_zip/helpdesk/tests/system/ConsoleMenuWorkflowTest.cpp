#include <gtest/gtest.h>
#include <sstream>
#include <functional>
#include <string>
#include "TestDatabaseFixture.h"
#include "DatabaseManager.h"
#include "User.h"
#include "Common.h"

// -----------------------------------------------------------------------
// Console menu workflow tests
// -----------------------------------------------------------------------
// These drive the *actual* User::showMenu() implementations -- the same
// code app/main.cpp calls -- by redirecting std::cin to a scripted
// input script and std::cout to a capture buffer, rather than only
// testing the DatabaseManager/Ticket layer underneath them. This is
// what gives the console-menu classes (Employee, Engineer, Admin)
// meaningful automated coverage instead of relying purely on manual
// testing.
//
// Every script below ends with the role's "Logout" choice so the menu
// loop terminates deterministically instead of spinning on exhausted
// stdin.
namespace {

// Runs `action` with std::cin fed from `scriptedInput` and std::cout
// captured into a string, restoring both streams afterward (even if
// `action` throws).
std::string runWithScriptedConsole(const std::string& scriptedInput,
                                    const std::function<void()>& action) {
    std::istringstream fakeIn(scriptedInput);
    std::ostringstream capturedOut;
    std::streambuf* originalIn = std::cin.rdbuf(fakeIn.rdbuf());
    std::streambuf* originalOut = std::cout.rdbuf(capturedOut.rdbuf());

    try {
        action();
    } catch (...) {
        std::cin.rdbuf(originalIn);
        std::cout.rdbuf(originalOut);
        throw;
    }

    std::cin.rdbuf(originalIn);
    std::cout.rdbuf(originalOut);
    return capturedOut.str();
}

} // namespace

TEST_F(TestDatabaseFixture, EmployeeMenu_RaiseTicketThenViewThenLogout) {
    int empId = db->addUser("Grace Employee", "grace", "pw", "grace@company.com", UserRole::EMPLOYEE);
    Employee employee(empId, "Grace Employee", "grace", "pw", "grace@company.com");

    std::string script =
        "1\n"                                   // Raise a new ticket
        "Broken monitor\n"                       // title
        "External monitor flickers constantly\n" // description
        "2\n"                                    // ticket type: Hardware
        "3\n"                                    // priority: HIGH
        "2\n"                                    // View my tickets
        "4\n";                                   // Logout

    std::string output = runWithScriptedConsole(script, [&]() { employee.showMenu(*db); });

    EXPECT_NE(output.find("Ticket created successfully"), std::string::npos);
    EXPECT_NE(output.find("Broken monitor"), std::string::npos);

    auto tickets = db->getTicketsByEmployee(empId);
    ASSERT_EQ(tickets.size(), 1u);
    EXPECT_EQ(tickets[0].getTitle(), "Broken monitor");
    EXPECT_EQ(tickets[0].getType(), TicketType::HARDWARE);
    EXPECT_EQ(tickets[0].getPriority(), TicketPriority::HIGH);
}

TEST_F(TestDatabaseFixture, EmployeeMenu_FeedbackFlowOnResolvedTicket) {
    int empId = db->addUser("Grace Employee", "grace", "pw", "grace@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Henry Engineer", "henry", "pw", "henry@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int ticketId = db->addTicket(t);
    Ticket loaded = db->getTicketById(ticketId);
    loaded.assignTo(engId);
    loaded.resolve("Fixed it.");
    db->updateTicket(loaded);

    Employee employee(empId, "Grace Employee", "grace", "pw", "grace@company.com");
    std::string script =
        "3\n" +                                    // Give feedback
        std::to_string(ticketId) + "\n"             // ticket ID
        "5\n"                                       // rating
        "Fast turnaround!\n"                        // comment
        "4\n";                                      // Logout

    runWithScriptedConsole(script, [&]() { employee.showMenu(*db); });

    Ticket rated = db->getTicketById(ticketId);
    EXPECT_EQ(rated.getFeedbackRating(), 5);
    EXPECT_EQ(rated.getFeedbackComment(), "Fast turnaround!");
}

TEST_F(TestDatabaseFixture, EngineerMenu_UpdateStatusThenResolveThenViewResolved) {
    int empId = db->addUser("Grace Employee", "grace", "pw", "grace@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Henry Engineer", "henry", "pw", "henry@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Slow database queries", "Reports time out", TicketType::DATABASE, TicketPriority::HIGH);
    int ticketId = db->addTicket(t);
    Ticket loaded = db->getTicketById(ticketId);
    loaded.assignTo(engId);
    db->updateTicket(loaded);

    Engineer engineer(engId, "Henry Engineer", "henry", "pw", "henry@company.com");
    std::string script =
        "1\n"                                        // View assigned tickets
        "3\n" +                                       // Update ticket status
        std::to_string(ticketId) + "\n"                // ticket ID
        "2\n"                                          // new status: IN_PROGRESS
        "4\n" +                                        // Resolve a ticket
        std::to_string(ticketId) + "\n"                 // ticket ID
        "Rebuilt the missing index.\n"                  // resolution notes
        "2\n"                                           // View resolved tickets
        "5\n";                                          // Logout

    std::string output = runWithScriptedConsole(script, [&]() { engineer.showMenu(*db); });

    EXPECT_NE(output.find("marked as RESOLVED"), std::string::npos);

    Ticket resolved = db->getTicketById(ticketId);
    EXPECT_EQ(resolved.getStatus(), TicketStatus::RESOLVED);
    EXPECT_EQ(resolved.getResolutionNotes(), "Rebuilt the missing index.");

    // Resolved-ticket workflow separation, exercised through the real
    // menu: it must no longer appear as "assigned".
    EXPECT_TRUE(db->getActiveTicketsByEngineer(engId).empty());
}

TEST_F(TestDatabaseFixture, EngineerMenu_CannotActOnTicketAssignedToSomeoneElse) {
    int empId = db->addUser("Grace Employee", "grace", "pw", "grace@company.com", UserRole::EMPLOYEE);
    int eng1 = db->addUser("Henry Engineer", "henry", "pw", "henry@company.com", UserRole::ENGINEER);
    int eng2 = db->addUser("Ivy Engineer", "ivy", "pw", "ivy@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int ticketId = db->addTicket(t);
    Ticket loaded = db->getTicketById(ticketId);
    loaded.assignTo(eng1); // belongs to Henry, not Ivy
    db->updateTicket(loaded);

    Engineer ivy(eng2, "Ivy Engineer", "ivy", "pw", "ivy@company.com");
    std::string script =
        "4\n" +                                // Resolve a ticket
        std::to_string(ticketId) + "\n"         // ticket ID (not hers)
        "5\n";                                  // Logout

    std::string output = runWithScriptedConsole(script, [&]() { ivy.showMenu(*db); });

    EXPECT_NE(output.find("[Error]"), std::string::npos);
    EXPECT_NE(output.find("not assigned to you"), std::string::npos);

    // Ticket must remain untouched.
    Ticket stillAssigned = db->getTicketById(ticketId);
    EXPECT_EQ(stillAssigned.getStatus(), TicketStatus::ASSIGNED);
    EXPECT_EQ(stillAssigned.getAssignedEngineerId(), eng1);
}

TEST_F(TestDatabaseFixture, AdminMenu_AssignTicketAddUserAndViewReports) {
    int adminId = db->authenticate("admin", "admin123").id;
    int empId = db->addUser("Grace Employee", "grace", "pw", "grace@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Henry Engineer", "henry", "pw", "henry@company.com", UserRole::ENGINEER);

    Ticket t(empId, "Cannot print", "Print spooler crashed", TicketType::SOFTWARE, TicketPriority::MEDIUM);
    int ticketId = db->addTicket(t);

    Admin admin(adminId, "System Administrator", "admin", "admin123", "admin@company.com");
    std::string script =
        "1\n"                                   // View all tickets
        "3\n" +                                  // Assign ticket to engineer
        std::to_string(ticketId) + "\n" +        // ticket ID
        std::to_string(engId) + "\n"             // engineer ID
        "4\n"                                    // Manage users
        "2\n"                                    // Add a new user
        "Ivy Employee\n"                         // name
        "ivy2\n"                                 // username
        "pw123\n"                                // password
        "ivy2@company.com\n"                     // email
        "1\n"                                    // role: EMPLOYEE
        "5\n"                                    // Reports & statistics
        "6\n";                                   // Logout

    std::string output = runWithScriptedConsole(script, [&]() { admin.showMenu(*db); });

    EXPECT_NE(output.find("assigned to Engineer"), std::string::npos);
    EXPECT_NE(output.find("User created with ID"), std::string::npos);
    EXPECT_NE(output.find("REPORTS & STATISTICS"), std::string::npos);

    Ticket assigned = db->getTicketById(ticketId);
    EXPECT_EQ(assigned.getStatus(), TicketStatus::ASSIGNED);
    EXPECT_EQ(assigned.getAssignedEngineerId(), engId);

    EXPECT_NO_THROW(db->authenticate("ivy2", "pw123"));
}

TEST_F(TestDatabaseFixture, AdminMenu_ViewResolvedTicketsShowsResolvedNotActive) {
    int adminId = db->authenticate("admin", "admin123").id;
    int empId = db->addUser("Grace Employee", "grace", "pw", "grace@company.com", UserRole::EMPLOYEE);
    int engId = db->addUser("Henry Engineer", "henry", "pw", "henry@company.com", UserRole::ENGINEER);

    Ticket active(empId, "Active ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int activeId = db->addTicket(active);
    Ticket loadedActive = db->getTicketById(activeId);
    loadedActive.assignTo(engId);
    db->updateTicket(loadedActive);

    Ticket resolved(empId, "Resolved ticket", "d", TicketType::OTHER, TicketPriority::LOW);
    int resolvedId = db->addTicket(resolved);
    Ticket loadedResolved = db->getTicketById(resolvedId);
    loadedResolved.assignTo(engId);
    loadedResolved.resolve("Fixed");
    db->updateTicket(loadedResolved);

    Admin admin(adminId, "System Administrator", "admin", "admin123", "admin@company.com");
    std::string script =
        "2\n"    // View resolved tickets
        "6\n";   // Logout

    std::string output = runWithScriptedConsole(script, [&]() { admin.showMenu(*db); });

    EXPECT_NE(output.find("Resolved ticket"), std::string::npos);
    EXPECT_EQ(output.find("Active ticket"), std::string::npos)
        << "the resolved-tickets view must not show still-active tickets";
}

TEST_F(TestDatabaseFixture, AdminMenu_InvalidMenuChoiceIsRejectedWithoutCrashing) {
    int adminId = db->authenticate("admin", "admin123").id;
    Admin admin(adminId, "System Administrator", "admin", "admin123", "admin@company.com");

    std::string script =
        "99\n"   // invalid choice
        "6\n";   // Logout

    std::string output = runWithScriptedConsole(script, [&]() { admin.showMenu(*db); });
    EXPECT_NE(output.find("Invalid choice"), std::string::npos);
}
