#include "User.h"
#include "DatabaseManager.h"
#include "InputUtil.h"
#include "Validation.h"
#include "PasswordInput.h"
#include <iostream>
#include <iomanip>

// =======================================================================
// User (base class)
// =======================================================================
User::User(int id, const std::string& name, const std::string& username,
           const std::string& password, const std::string& email, UserRole role)
    : id(id), name(name), username(username), password(password), email(email), role(role) {}

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
    std::cout << "Select new status:\n"
              << "  1. ASSIGNED\n  2. IN_PROGRESS\n  3. RESOLVED (use option 3 in the main menu instead)\n";
    int choice = InputUtil::readInt("Choice: ");
    switch (choice) {
        case 1: ticket.updateStatus(TicketStatus::ASSIGNED); break;
        case 2: ticket.updateStatus(TicketStatus::IN_PROGRESS); break;
        default:
            std::cout << "Use the 'Resolve a ticket' menu option to mark a ticket resolved.\n";
            return;
    }
    db.updateTicket(ticket);
    std::cout << "Ticket status updated.\n";
}

void Engineer::resolveTicketFlow(DatabaseManager& db) {
    std::cout << "\n-- Resolve Ticket --\n";
    int ticketId = InputUtil::readInt("Enter Ticket ID: ");
    Ticket ticket = db.getTicketById(ticketId);
    if (ticket.getAssignedEngineerId() != id) {
        throw ValidationException("This ticket is not assigned to you.");
    }
    std::string notes = InputUtil::readNonEmptyLine("Resolution notes: ");
    ticket.resolve(notes);
    db.updateTicket(ticket);
    std::cout << "Ticket #" << ticketId << " marked as RESOLVED.\n";
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
    Ticket ticket = db.getTicketById(ticketId);

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

    ticket.assignTo(engineerId);
    db.updateTicket(ticket);
    std::cout << "Ticket #" << ticketId << " assigned to Engineer #" << engineerId << ".\n";
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
