#ifndef USER_H
#define USER_H

#include <string>
#include "Common.h"

class DatabaseManager; // forward declaration (avoids circular include)

// -----------------------------------------------------------------------
// User (abstract base class)
// -----------------------------------------------------------------------
// Encapsulates the fields common to every account in the system and
// declares a pure virtual showMenu() method. Each derived role
// (Employee, Engineer, Admin) implements its own console menu,
// demonstrating runtime polymorphism: code that holds a User* / User&
// can call showMenu() without knowing the concrete role, and the
// correct role-specific behavior executes.
// -----------------------------------------------------------------------
class User {
protected:
    int id;
    std::string name;
    std::string username;
    std::string password;   // NOTE: stored as-is for demo purposes; a real
                             // system must hash + salt passwords.
    std::string email;
    UserRole role;

public:
    User(int id, const std::string& name, const std::string& username,
         const std::string& password, const std::string& email, UserRole role);

    // --- Encapsulated accessors ---
    int getId() const { return id; }
    const std::string& getName() const { return name; }
    const std::string& getUsername() const { return username; }
    const std::string& getEmail() const { return email; }
    UserRole getRole() const { return role; }

    // Password check kept inside the class rather than exposing the
    // raw password field to callers.
    bool checkPassword(const std::string& attempt) const;

    void setId(int newId) { id = newId; }

    // Common info display, can be overridden/extended by subclasses.
    virtual void displayInfo() const;

    // Pure virtual -> makes User an abstract class. Each role provides
    // its own interactive console menu / workflow.
    virtual void showMenu(DatabaseManager& db) = 0;

    virtual ~User() = default;
};

// -----------------------------------------------------------------------
// Employee: can raise tickets, view own tickets, give feedback.
// -----------------------------------------------------------------------
class Employee : public User {
public:
    Employee(int id, const std::string& name, const std::string& username,
              const std::string& password, const std::string& email);

    void showMenu(DatabaseManager& db) override;

private:
    void createTicketFlow(DatabaseManager& db);
    void viewMyTicketsFlow(DatabaseManager& db);
    void giveFeedbackFlow(DatabaseManager& db);
};

// -----------------------------------------------------------------------
// Engineer: can view assigned tickets, update status, resolve tickets.
// -----------------------------------------------------------------------
class Engineer : public User {
public:
    Engineer(int id, const std::string& name, const std::string& username,
              const std::string& password, const std::string& email);

    void showMenu(DatabaseManager& db) override;

private:
    void viewAssignedTicketsFlow(DatabaseManager& db);   // active only: ASSIGNED / IN_PROGRESS
    void viewResolvedTicketsFlow(DatabaseManager& db);    // resolved workflow, separate view
    void updateStatusFlow(DatabaseManager& db);
    void resolveTicketFlow(DatabaseManager& db);
};

// -----------------------------------------------------------------------
// Admin: can view all tickets, assign tickets to engineers, manage
// users, and generate statistics/reports.
// -----------------------------------------------------------------------
class Admin : public User {
public:
    Admin(int id, const std::string& name, const std::string& username,
           const std::string& password, const std::string& email);

    void showMenu(DatabaseManager& db) override;

private:
    void viewAllTicketsFlow(DatabaseManager& db);
    void viewResolvedTicketsFlow(DatabaseManager& db);
    void assignTicketFlow(DatabaseManager& db);
    void manageUsersFlow(DatabaseManager& db);
    void reportsFlow(DatabaseManager& db);
};

#endif // USER_H
