#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sqlite3.h>
#include "Common.h"
#include "Ticket.h"

class User; // forward declaration

// Lightweight struct used to transfer raw user rows out of the database
// layer before the caller reconstructs the correct User subclass.
struct UserRecord {
    int id;
    std::string name;
    std::string username;
    std::string password;
    std::string email;
    UserRole role;
};

// -----------------------------------------------------------------------
// DatabaseManager
// -----------------------------------------------------------------------
// Wraps all SQLite3 C-API calls behind a clean C++ interface. This is the
// only class in the project that directly touches sqlite3*/sqlite3_stmt*
// pointers, keeping persistence concerns cleanly separated from the
// domain classes (User, Ticket) -- a simple Repository/DAO pattern.
//
// Every public method that talks to the database validates the result
// codes returned by SQLite and throws a DatabaseException on failure,
// satisfying the "Exception Handling" requirement for all DB operations.
// -----------------------------------------------------------------------
class DatabaseManager {
private:
    sqlite3* db;
    std::string dbPath;

    // Executes a plain SQL statement with no result set (CREATE TABLE, etc.)
    void execute(const std::string& sql);

public:
    explicit DatabaseManager(const std::string& path);
    ~DatabaseManager();

    // Non-copyable (owns a raw sqlite3* handle)
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void connect();                 // Opens the connection, throws DatabaseException on failure
    void initializeSchema();        // Creates tables if they do not already exist
    void close();

    // ---------------- User operations ----------------
    int addUser(const std::string& name, const std::string& username,
                const std::string& password, const std::string& email,
                UserRole role);                                   // returns new user id
    UserRecord authenticate(const std::string& username, const std::string& password);
    std::vector<UserRecord> getAllUsers();
    std::vector<UserRecord> getUsersByRole(UserRole role);
    UserRecord getUserById(int id);
    bool usernameExists(const std::string& username);
    bool emailExists(const std::string& email);

    // ---------------- Ticket operations ----------------
    int addTicket(const Ticket& ticket);                          // returns new ticket id
    void updateTicket(const Ticket& ticket);                      // persists full ticket state
    Ticket getTicketById(int ticketId);
    std::vector<Ticket> getAllTickets();
    std::vector<Ticket> getTicketsByEmployee(int employeeId);
    std::vector<Ticket> getTicketsByEngineer(int engineerId);
    std::vector<Ticket> getTicketsByStatus(TicketStatus status);

    // --- Resolved-ticket workflow separation ---
    // Filtering happens at the query level (SQL WHERE clauses) rather
    // than by fetching everything and filtering in C++, so there is a
    // single, authoritative definition of "active" vs "resolved" and no
    // duplicated filtering logic across callers.
    static constexpr const char* ACTIVE_STATUS_CLAUSE =
        "status IN ('ASSIGNED','IN_PROGRESS')";
    static constexpr const char* RESOLVED_STATUS_CLAUSE =
        "status IN ('RESOLVED','CLOSED')";

    std::vector<Ticket> getActiveTicketsByEngineer(int engineerId);   // ASSIGNED / IN_PROGRESS only
    std::vector<Ticket> getResolvedTicketsByEngineer(int engineerId); // RESOLVED / CLOSED only
    std::vector<Ticket> getAllResolvedTickets();                     // RESOLVED / CLOSED, any engineer

    // ---------------- Reporting ----------------
    std::map<std::string, int> getTicketCountByStatus();
    std::map<std::string, int> getTicketCountByPriority();
    double getAverageFeedbackRating();
    int getTotalTicketCount();
};

#endif // DATABASE_MANAGER_H
