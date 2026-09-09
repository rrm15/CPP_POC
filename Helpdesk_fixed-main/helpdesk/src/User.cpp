#include "User.h"
#include "DatabaseManager.h"
#include "InputUtil.h"
#include "Validation.h"
#include "PasswordInput.h"
#include "PasswordHasher.h"
#include <iostream>
#include <iomanip>

// =======================================================================
// User (base class)
// =======================================================================
User::User(int id, const std::string& name, const std::string& username,
           const std::string& password, const std::string& email, UserRole role)
    : id(id), name(name), username(username), password(password), email(email), role(role) {}

bool User::checkPassword(const std::string& attempt) const {
    return PasswordHasher::verifyPassword(attempt, password);
}

void User::displayInfo() const {
    std::cout << "Name: " << name << " | Username: " << username
              << " | Role: " << roleToString(role) << " | Email: " << email << "\n";
}

// =======================================================================
// Employee
// =======================================================================
Employee::Employee(int id, const std::string& name, const std::string& username,
                    const std::string& password, const std::string& email)
    : User(id, name, username, password, email, UserRole::EMPLOYEE) {}

void Employee::showMenu(DatabaseManager& db) {
    bool running = true;
    while (running) {
        std::cout << "\n===== EMPLOYEE MENU (" << name << ") =====\n"
                  << "1. Raise a new ticket\n"
                  << "2. View my tickets\n"
                  << "3. Give feedback on a resolved ticket\n"
                  << "4. Logout\n";
        int choice = InputUtil::readInt("Choice: ");
        try {
            switch (choice) {
                case 1: createTicketFlow(db); break;
                case 2: viewMyTicketsFlow(db); break;
                case 3: giveFeedbackFlow(db); break;
                case 4: running = false; break;
                default: std::cout << "Invalid choice.\n";
            }
        } catch (const AppException& ex) {
            // Catches ValidationException, NotFoundException, DatabaseException, etc.
            std::cout << "[Error] " << ex.what() << "\n";
        }
    }
}

void Employee::createTicketFlow(DatabaseManager& db) {
    std::cout << "\n-- Raise New Ticket --\n";
    std::string title = InputUtil::readNonEmptyLine("Title: ");
    std::string description = InputUtil::readNonEmptyLine("Description: ");
    TicketType type = InputUtil::readTicketType();
    TicketPriority priority = InputUtil::readPriority();

    Ticket ticket(id, title, description, type, priority);
    int newId = db.addTicket(ticket);
    std::cout << "Ticket created successfully with ID #" << newId << ".\n";
}

void Employee::viewMyTicketsFlow(DatabaseManager& db) {
    std::cout << "\n-- My Tickets --\n";
    auto tickets = db.getTicketsByEmployee(id);
    if (tickets.empty()) {
        std::cout << "You have not raised any tickets yet.\n";
        return;
    }
    for (const auto& t : tickets) t.display();
}

void Employee::giveFeedbackFlow(DatabaseManager& db) {
    std::cout << "\n-- Give Feedback --\n";
    auto tickets = db.getTicketsByEmployee(id);
    bool anyResolved = false;
    for (const auto& t : tickets) {
        if (t.getStatus() == TicketStatus::RESOLVED || t.getStatus() == TicketStatus::CLOSED) {
            t.display();
            anyResolved = true;
        }
    }
    if (!anyResolved) {
        std::cout << "You have no resolved tickets to give feedback on.\n";
        return;
    }
    int ticketId = InputUtil::readInt("Enter Ticket ID to give feedback on: ");
    Ticket ticket = db.getTicketById(ticketId);
    if (ticket.getEmployeeId() != id) {
        throw ValidationException("That ticket does not belong to you.");
    }
    int rating = InputUtil::readInt("Rating (1-5): ");
    std::string comment = InputUtil::readLine("Comment (optional): ");
    ticket.addFeedback(rating, comment);
    db.updateTicket(ticket);
    std::cout << "Thank you! Feedback recorded.\n";
}

// =======================================================================
// Engineer
// =======================================================================
Engineer::Engineer(int id, const std::string& name, const std::string& username,
                    const std::string& password, const std::string& email)
    : User(id, name, username, password, email, UserRole::ENGINEER) {}

void Engineer::showMenu(DatabaseManager& db) {
    bool running = true;
    while (running) {
        std::cout << "\n===== ENGINEER MENU (" << name << ") =====\n"
                  << "1. View assigned tickets\n"
                  << "2. View resolved tickets\n"
                  << "3. Update ticket status\n"
                  << "4. Resolve a ticket\n"
                  << "5. Logout\n";
        int choice = InputUtil::readInt("Choice: ");
        try {
            switch (choice) {
                case 1: viewAssignedTicketsFlow(db); break;
                case 2: viewResolvedTicketsFlow(db); break;
                case 3: updateStatusFlow(db); break;
                case 4: resolveTicketFlow(db); break;
                case 5: running = false; break;
                default: std::cout << "Invalid choice.\n";
            }
        } catch (const LockConflictException& ex) {
            std::cout << "[Conflict] " << ex.what() << "\n";
        } catch (const AppException& ex) {
            std::cout << "[Error] " << ex.what() << "\n";
        }
    }
}

void Engineer::viewAssignedTicketsFlow(DatabaseManager& db) {
    // "Assigned Tickets" shows only tickets still in progress (ASSIGNED /
    // IN_PROGRESS). Resolved/closed tickets are deliberately excluded --
    // see viewResolvedTicketsFlow() -- and the filter is applied at the
    // SQL query level (DatabaseManager::getActiveTicketsByEngineer), not
    // by fetching everything and filtering here.
    std::cout << "\n-- Tickets Assigned To Me (Active) --\n";
    auto tickets = db.getActiveTicketsByEngineer(id);
    if (tickets.empty()) {
        std::cout << "No active tickets are currently assigned to you.\n";
        return;
    }
    for (const auto& t : tickets) t.display();
}

void Engineer::viewResolvedTicketsFlow(DatabaseManager& db) {
    std::cout << "\n-- Tickets I Have Resolved --\n";
    auto tickets = db.getResolvedTicketsByEngineer(id);
    if (tickets.empty()) {
        std::cout << "No tickets resolved.\n";
        return;
    }
    for (const auto& t : tickets) t.display();
}

void Engineer::updateStatusFlow(DatabaseManager& db) {
    std::cout << "\n-- Update Ticket Status --\n";
    int ticketId = InputUtil::readInt("Enter Ticket ID: ");
    Ticket ticket = db.getTicketById(ticketId);
    if (ticket.getAssignedEngineerId() != id) {
        throw ValidationException("This ticket is not assigned to you.");
    }
    std::cout << "Current status: " << statusToString(ticket.getStatus()) << "\n";
    std::cout << "Move to IN_PROGRESS? (1=Yes, other=Cancel): ";
    int choice = InputUtil::readInt("");
    if (choice != 1) {
        std::cout << "Cancelled.\n";
        return;
    }
    // Atomic: only succeeds if engineer still owns this ticket and it is
    // still ASSIGNED. Zero rows changed means stale data (already moved
    // or re-assigned by another session).
    bool changed = db.atomicUpdateStatusInProgress(ticketId, id);
    if (!changed) {
        std::cout << "Ticket could not be updated. It may have already changed status "
                     "or been re-assigned. Please refresh and retry.\n";
    } else {
        std::cout << "Ticket status moved to IN_PROGRESS.\n";
    }
}

void Engineer::resolveTicketFlow(DatabaseManager& db) {
    std::cout << "\n-- Resolve Ticket --\n";
    int ticketId = InputUtil::readInt("Enter Ticket ID: ");
    // Verify ownership before asking for notes (avoids wasted input).
    Ticket ticket = db.getTicketById(ticketId);
    if (ticket.getAssignedEngineerId() != id) {
        throw ValidationException("This ticket is not assigned to you.");
    }
    if (ticket.getStatus() == TicketStatus::RESOLVED ||
        ticket.getStatus() == TicketStatus::CLOSED) {
        std::cout << "Ticket #" << ticketId << " is already resolved.\n";
        return;
    }
    std::string notes = InputUtil::readNonEmptyLine("Resolution notes: ");
    // Atomic: only resolves if this engineer still owns the ticket and
    // status is ASSIGNED or IN_PROGRESS. Guards against repeated calls
    // and wrong-engineer attempts at the database level.
    bool changed = db.atomicResolveTicket(ticketId, id, notes);
    if (!changed) {
        std::cout << "Ticket could not be resolved. It may have already been resolved, "
                     "re-assigned, or does not exist. Refresh and retry.\n";
    } else {
        std::cout << "Ticket #" << ticketId << " marked as RESOLVED.\n";
    }
}

// =======================================================================
// Admin
// =======================================================================
Admin::Admin(int id, const std::string& name, const std::string& username,
             const std::string& password, const std::string& email)
    : User(id, name, username, password, email, UserRole::ADMIN) {}

void Admin::showMenu(DatabaseManager& db) {
    bool running = true;
    while (running) {
        std::cout << "\n===== ADMIN MENU (" << name << ") =====\n"
                  << "1. View all tickets\n"
                  << "2. View resolved tickets\n"
                  << "3. Assign ticket to engineer\n"
                  << "4. Manage users (list / add)\n"
                  << "5. Reports & statistics\n"
                  << "6. Logout\n";
        int choice = InputUtil::readInt("Choice: ");
        try {
            switch (choice) {
                case 1: viewAllTicketsFlow(db); break;
                case 2: viewResolvedTicketsFlow(db); break;
                case 3: assignTicketFlow(db); break;
                case 4: manageUsersFlow(db); break;
                case 5: reportsFlow(db); break;
                case 6: running = false; break;
                default: std::cout << "Invalid choice.\n";
            }
        } catch (const LockConflictException& ex) {
            std::cout << "[Conflict] " << ex.what() << "\n";
        } catch (const AppException& ex) {
            std::cout << "[Error] " << ex.what() << "\n";
        }
    }
}

void Admin::viewAllTicketsFlow(DatabaseManager& db) {
    std::cout << "\n-- All Tickets --\n";
    auto tickets = db.getAllTickets();
    if (tickets.empty()) {
        std::cout << "No tickets in the system yet.\n";
        return;
    }
    for (const auto& t : tickets) t.display();
}

void Admin::viewResolvedTicketsFlow(DatabaseManager& db) {
    std::cout << "\n-- Resolved Tickets (All Engineers) --\n";
    auto tickets = db.getAllResolvedTickets();
    if (tickets.empty()) {
        std::cout << "No tickets resolved.\n";
        return;
    }
    for (const auto& t : tickets) t.display();
}

void Admin::assignTicketFlow(DatabaseManager& db) {
    std::cout << "\n-- Assign Ticket to Engineer --\n";
    auto openTickets = db.getTicketsByStatus(TicketStatus::OPEN);
    if (openTickets.empty()) {
        std::cout << "There are no OPEN (unassigned) tickets right now.\n";
        return;
    }
    for (const auto& t : openTickets) t.display();

    int ticketId = InputUtil::readInt("Enter Ticket ID to assign: ");

    auto engineers = db.getUsersByRole(UserRole::ENGINEER);
    if (engineers.empty()) {
        std::cout << "No engineers exist in the system yet. Add one first.\n";
        return;
    }
    std::cout << "Available engineers:\n";
    for (const auto& e : engineers) {
        std::cout << "  ID " << e.id << " - " << e.name << " (" << e.username << ")\n";
    }
    int engineerId = InputUtil::readInt("Enter Engineer ID to assign this ticket to: ");

    // Atomic: only assigns when ticket is still OPEN and unassigned.
    // Guards against a second admin session racing to assign the same ticket.
    bool changed = db.atomicAssignTicket(ticketId, engineerId);
    if (!changed) {
        std::cout << "Ticket #" << ticketId
                  << " could not be assigned. It may already be assigned, "
                     "no longer OPEN, or may not exist. Refresh the list and retry.\n";
    } else {
        std::cout << "Ticket #" << ticketId << " assigned to Engineer #" << engineerId << ".\n";
    }
}

void Admin::manageUsersFlow(DatabaseManager& db) {
    std::cout << "\n-- Manage Users --\n"
              << "1. List all users\n"
              << "2. Add a new user\n";
    int choice = InputUtil::readInt("Choice: ");
    if (choice == 1) {
        auto users = db.getAllUsers();
        for (const auto& u : users) {
            std::cout << "  ID " << u.id << " | " << u.name << " | " << u.username
                      << " | " << roleToString(u.role) << " | " << u.email << "\n";
        }
    } else if (choice == 2) {
        std::string name = InputUtil::readNonEmptyLine("Full name: ");
        std::string username = InputUtil::readNonEmptyLine("Username: ");
        Validation::validateUsernameOrThrow(username);
        std::string password = PasswordInput::readMaskedNonEmptyPassword("Password: ");
        std::string email = InputUtil::readNonEmptyLine("Email: ");
        Validation::validateEmailOrThrow(email);
        std::cout << "Role: 1. EMPLOYEE  2. ENGINEER  3. ADMIN\n";
        int roleChoice = InputUtil::readInt("Choice: ");
        UserRole role;
        switch (roleChoice) {
            case 1: role = UserRole::EMPLOYEE; break;
            case 2: role = UserRole::ENGINEER; break;
            case 3: role = UserRole::ADMIN; break;
            default: throw ValidationException("Invalid role choice.");
        }
        int newId = db.addUser(name, username, password, email, role);
        std::cout << "User created with ID #" << newId << ".\n";
    } else {
        std::cout << "Invalid choice.\n";
    }
}

void Admin::reportsFlow(DatabaseManager& db) {
    std::cout << "\n===== REPORTS & STATISTICS =====\n";
    std::cout << "Total tickets: " << db.getTotalTicketCount() << "\n\n";

    std::cout << "By Status:\n";
    for (const auto& [status, count] : db.getTicketCountByStatus()) {
        std::cout << "  " << std::left << std::setw(12) << status << ": " << count << "\n";
    }

    std::cout << "\nBy Priority:\n";
    for (const auto& [priority, count] : db.getTicketCountByPriority()) {
        std::cout << "  " << std::left << std::setw(12) << priority << ": " << count << "\n";
    }

    double avgRating = db.getAverageFeedbackRating();
    std::cout << "\nAverage customer feedback rating: "
              << std::fixed << std::setprecision(2) << avgRating << " / 5.0\n";
}
