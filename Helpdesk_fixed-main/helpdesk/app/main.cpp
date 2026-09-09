// =========================================================================
// IT Helpdesk Ticket Management System
// -------------------------------------------------------------------------
// Console-based C++ application demonstrating:
//   - OOP (abstract base class User, derived Employee/Engineer/Admin,
//     encapsulated Ticket and DatabaseManager classes)
//   - Runtime polymorphism (User::showMenu is pure virtual)
//   - STL containers (std::vector, std::map)
//   - SQLite3 database integration (DatabaseManager)
//   - Exception handling (custom exception hierarchy in Common.h)
// =========================================================================

#include <iostream>
#include <memory>
#include <sqlite3.h>
#include "DatabaseManager.h"
#include "User.h"
#include "InputUtil.h"
#include "Common.h"
#include "PasswordInput.h"
#include "Validation.h"

// Factory function: given a raw UserRecord loaded from the database,
// construct the correct polymorphic User subclass. Centralizing this
// avoids scattering role-branching logic throughout the codebase.
static std::unique_ptr<User> makeUser(const UserRecord& rec) {
    switch (rec.role) {
        case UserRole::EMPLOYEE:
            return std::make_unique<Employee>(rec.id, rec.name, rec.username, rec.password, rec.email);
        case UserRole::ENGINEER:
            return std::make_unique<Engineer>(rec.id, rec.name, rec.username, rec.password, rec.email);
        case UserRole::ADMIN:
            return std::make_unique<Admin>(rec.id, rec.name, rec.username, rec.password, rec.email);
    }
    throw ValidationException("Unknown role encountered while constructing user.");
}

static void printBanner() {
    std::cout << "=========================================================\n";
    std::cout << "     IT HELPDESK TICKET MANAGEMENT SYSTEM (C++/SQLite)   \n";
    std::cout << "=========================================================\n";
}

static void registerFlow(DatabaseManager& db) {
    std::cout << "\n-- Employee Self-Registration --\n";
    try {
        std::string name = InputUtil::readNonEmptyLine("Full name: ");
        std::string username = InputUtil::readNonEmptyLine("Choose a username: ");
        Validation::validateUsernameOrThrow(username);
        std::string password = PasswordInput::readMaskedNonEmptyPassword("Choose a password: ");
        std::string email = InputUtil::readNonEmptyLine("Email: ");
        Validation::validateEmailOrThrow(email);
        // Self-registration is limited to the Employee role; Engineer and
        // Admin accounts are provisioned by an existing Admin (see
        // Admin::manageUsersFlow) to keep support-staff onboarding
        // controlled.
        int newId = db.addUser(name, username, password, email, UserRole::EMPLOYEE);
        std::cout << "Registration successful! Your user ID is " << newId
                  << ". You can now log in.\n";
    } catch (const AppException& ex) {
        std::cout << "[Error] " << ex.what() << "\n";
    }
}

static std::unique_ptr<User> loginFlow(DatabaseManager& db) {
    std::cout << "\n-- Login --\n";
    std::string username = InputUtil::readNonEmptyLine("Username: ");
    std::string password = PasswordInput::readMaskedNonEmptyPassword("Password: ");
    try {
        UserRecord rec = db.authenticate(username, password);
        std::cout << "Login successful. Welcome, " << rec.name
                  << " (" << roleToString(rec.role) << ")!\n";
        return makeUser(rec);
    } catch (const AuthenticationException& ex) {
        std::cout << "[Login Failed] " << ex.what() << "\n";
        return nullptr;
    }
}

int main() {
    // This application is genuinely single-threaded end to end (no
    // std::thread/pthread is ever created), and no DatabaseManager /
    // sqlite3 connection is ever shared across threads. Telling SQLite
    // that up front lets it skip its internal mutex bookkeeping entirely,
    // which is both a legitimate perf win and eliminates a class of
    // Helgrind false positives (SQLite's mutex does its own recursion
    // tracking on top of a plain pthread mutex in a way Helgrind's
    // interceptor misreads as an illegal recursive lock). This MUST run
    // before the first sqlite3 API call anywhere in the process --
    // sqlite3_config() only succeeds prior to SQLite's implicit
    // first-use initialization.
    sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

    printBanner();

    // The DatabaseManager owns the sqlite3 connection for the whole
    // program lifetime. All setup happens inside try/catch so any
    // failure to open/initialize the DB is reported cleanly instead of
    // crashing the program.
    DatabaseManager db("helpdesk.db");
    try {
        db.connect();
        db.initializeSchema();
    } catch (const DatabaseException& ex) {
        std::cerr << "Fatal: could not initialize database.\n" << ex.what() << "\n";
        return 1;
    }

    std::cout << "\nDefault admin account -> username: admin | password: admin123\n";

    bool exitProgram = false;
    while (!exitProgram) {
        std::cout << "\n===== MAIN MENU =====\n"
                  << "1. Login\n"
                  << "2. Register (Employee)\n"
                  << "3. Exit\n";
        int choice = InputUtil::readInt("Choice: ");

        switch (choice) {
            case 1: {
                std::unique_ptr<User> user = loginFlow(db);
                if (user) {
                    // Polymorphic dispatch: the concrete showMenu()
                    // implementation (Employee/Engineer/Admin) runs here,
                    // even though 'user' is declared as a base User*.
                    try {
                        user->showMenu(db);
                    } catch (const AppException& ex) {
                        std::cout << "[Error] " << ex.what() << "\n";
                    }
                }
                break;
            }
            case 2:
                registerFlow(db);
                break;
            case 3:
                exitProgram = true;
                break;
            default:
                std::cout << "Invalid choice. Please select 1-3.\n";
        }
    }

    std::cout << "\nThank you for using the IT Helpdesk Ticket Management System. Goodbye!\n";
    db.close();
    return 0;
}
