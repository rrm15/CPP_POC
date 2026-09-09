#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "Common.h"
#include <sqlite3.h>
#include <string>
#include <vector>
#include <memory>

class DatabaseManager {
public:
    static DatabaseManager& getInstance();
    
    // Database initialization
    bool initialize(const std::string& db_path);
    bool close();
    
    // User operations (conditional writes to prevent overwrites)
    int insertUser(const std::string& username, const std::string& email, UserRole role);
    User* getUserByUsername(const std::string& username);
    bool updateUserPassword(int user_id, const std::string& password);
    
    // Ticket operations (conditional updates for concurrent safety)
    int insertTicket(int customer_id, const std::string& title, const std::string& description);
    Ticket* getTicketById(int ticket_id);
    std::vector<Ticket> getTicketsByStatus(TicketStatus status);
    bool conditionalAssignTicket(int ticket_id, int engineer_id);
    bool conditionalUpdateStatus(int ticket_id, TicketStatus new_status);
    bool conditionalResolveTicket(int ticket_id, const std::string& resolution);
    
    // Transaction support
    bool beginTransaction();
    bool commit();
    bool rollback();
    
private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    sqlite3* db;
    std::string last_error;
    
    // Helper methods
    bool executeSql(const std::string& sql);
    sqlite3_stmt* prepareStatement(const std::string& sql);
};

#endif  // DATABASE_MANAGER_H
