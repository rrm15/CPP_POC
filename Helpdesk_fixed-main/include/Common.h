#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>

// Enums for ticket and user states
enum class UserRole {
    ADMIN,
    ENGINEER,
    CUSTOMER
};

enum class TicketStatus {
    OPEN,
    ASSIGNED,
    IN_PROGRESS,
    RESOLVED
};

// Common structures
struct User {
    int id;
    std::string username;
    std::string email;
    UserRole role;
    std::string password;  // Plaintext during validation only
};

struct Ticket {
    int id;
    std::string title;
    std::string description;
    TicketStatus status;
    int customer_id;
    int assigned_engineer_id;
    std::string resolution;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

#endif  // COMMON_H
