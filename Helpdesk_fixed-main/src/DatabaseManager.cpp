#include "../include/DatabaseManager.h"
#include <iostream>
#include <sstream>

DatabaseManager::DatabaseManager() : db(nullptr) {}

DatabaseManager::~DatabaseManager() {
    close();
}

DatabaseManager& DatabaseManager::getInstance() {
    static DatabaseManager instance;
    return instance;
}

bool DatabaseManager::initialize(const std::string& db_path) {
    int rc = sqlite3_open_v2(db_path.c_str(), &db, 
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                            nullptr);
    
    if (rc != SQLITE_OK) {
        last_error = sqlite3_errmsg(db);
        return false;
    }
    
    // Create tables if they don't exist
    const std::string schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            username TEXT UNIQUE NOT NULL,
            email TEXT UNIQUE NOT NULL,
            role INTEGER NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        CREATE TABLE IF NOT EXISTS tickets (
            id INTEGER PRIMARY KEY,
            customer_id INTEGER NOT NULL,
            assigned_engineer_id INTEGER,
            title TEXT NOT NULL,
            description TEXT NOT NULL,
            status INTEGER NOT NULL,
            resolution TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY(customer_id) REFERENCES users(id),
            FOREIGN KEY(assigned_engineer_id) REFERENCES users(id)
        );
    )";
    
    return executeSql(schema);
}

bool DatabaseManager::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
    return true;
}

int DatabaseManager::insertUser(const std::string& username, const std::string& email, UserRole role) {
    std::stringstream sql;
    sql << "INSERT INTO users (username, email, role) VALUES ('" 
        << username << "', '" << email << "', " << static_cast<int>(role) << ");";
    
    if (!executeSql(sql.str())) {
        return -1;
    }
    return sqlite3_last_insert_rowid(db);
}

User* DatabaseManager::getUserByUsername(const std::string& username) {
    std::stringstream sql;
    sql << "SELECT id, username, email, role FROM users WHERE username = '" << username << "';";
    
    sqlite3_stmt* stmt = prepareStatement(sql.str());
    if (!stmt) return nullptr;
    
    User* user = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user = new User();
        user->id = sqlite3_column_int(stmt, 0);
        user->username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user->email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user->role = static_cast<UserRole>(sqlite3_column_int(stmt, 3));
    }
    
    sqlite3_finalize(stmt);
    return user;
}

bool DatabaseManager::updateUserPassword(int user_id, const std::string& password) {
    // Password validation only, no storage
    return true;
}

int DatabaseManager::insertTicket(int customer_id, const std::string& title, const std::string& description) {
    std::stringstream sql;
    sql << "INSERT INTO tickets (customer_id, title, description, status) "
        << "VALUES (" << customer_id << ", '" << title << "', '" << description << "', 0);";
    
    if (!executeSql(sql.str())) {
        return -1;
    }
    return sqlite3_last_insert_rowid(db);
}

Ticket* DatabaseManager::getTicketById(int ticket_id) {
    std::stringstream sql;
    sql << "SELECT id, customer_id, assigned_engineer_id, title, description, status FROM tickets WHERE id = " << ticket_id << ";";
    
    sqlite3_stmt* stmt = prepareStatement(sql.str());
    if (!stmt) return nullptr;
    
    Ticket* ticket = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ticket = new Ticket();
        ticket->id = sqlite3_column_int(stmt, 0);
        ticket->customer_id = sqlite3_column_int(stmt, 1);
        ticket->assigned_engineer_id = sqlite3_column_int(stmt, 2);
        ticket->title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        ticket->description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        ticket->status = static_cast<TicketStatus>(sqlite3_column_int(stmt, 5));
    }
    
    sqlite3_finalize(stmt);
    return ticket;
}

std::vector<Ticket> DatabaseManager::getTicketsByStatus(TicketStatus status) {
    std::vector<Ticket> result;
    std::stringstream sql;
    sql << "SELECT id, customer_id, assigned_engineer_id, title, description, status FROM tickets WHERE status = " << static_cast<int>(status) << ";";
    
    sqlite3_stmt* stmt = prepareStatement(sql.str());
    if (!stmt) return result;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Ticket ticket;
        ticket.id = sqlite3_column_int(stmt, 0);
        ticket.customer_id = sqlite3_column_int(stmt, 1);
        ticket.assigned_engineer_id = sqlite3_column_int(stmt, 2);
        ticket.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        ticket.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        ticket.status = static_cast<TicketStatus>(sqlite3_column_int(stmt, 5));
        result.push_back(ticket);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::conditionalAssignTicket(int ticket_id, int engineer_id) {
    // Conditional update: only assign if currently OPEN
    std::stringstream sql;
    sql << "UPDATE tickets SET assigned_engineer_id = " << engineer_id << ", status = 1 "
        << "WHERE id = " << ticket_id << " AND status = 0;";
    
    return executeSql(sql.str());
}

bool DatabaseManager::conditionalUpdateStatus(int ticket_id, TicketStatus new_status) {
    std::stringstream sql;
    sql << "UPDATE tickets SET status = " << static_cast<int>(new_status) 
        << ", updated_at = CURRENT_TIMESTAMP WHERE id = " << ticket_id << ";";
    
    return executeSql(sql.str());
}

bool DatabaseManager::conditionalResolveTicket(int ticket_id, const std::string& resolution) {
    std::stringstream sql;
    sql << "UPDATE tickets SET status = 3, resolution = '" << resolution 
        << "', updated_at = CURRENT_TIMESTAMP WHERE id = " << ticket_id << " AND status != 3;";
    
    return executeSql(sql.str());
}

bool DatabaseManager::beginTransaction() {
    return executeSql("BEGIN TRANSACTION;");
}

bool DatabaseManager::commit() {
    return executeSql("COMMIT;");
}

bool DatabaseManager::rollback() {
    return executeSql("ROLLBACK;");
}

bool DatabaseManager::executeSql(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    
    if (rc != SQLITE_OK) {
        last_error = err ? std::string(err) : "Unknown error";
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

sqlite3_stmt* DatabaseManager::prepareStatement(const std::string& sql) {
    sqlite3_stmt* stmt = nullptr;
    const char* unused = nullptr;
    
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, &unused);
    if (rc != SQLITE_OK) {
        last_error = sqlite3_errmsg(db);
        return nullptr;
    }
    return stmt;
}
