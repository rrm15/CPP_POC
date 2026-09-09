#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

enum class UserRole {
    EMPLOYEE,
    ENGINEER,
    ADMIN
};

enum class TicketStatus {
    OPEN,
    ASSIGNED,
    IN_PROGRESS,
    RESOLVED,
    CLOSED
};

enum class TicketPriority {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

// Classification of the subject-matter area a ticket belongs to.
// Introduced so tickets can be filtered/reported on by category in
// addition to status and priority.
enum class TicketType {
    SOFTWARE,
    HARDWARE,
    INFOSEC,
    NETWORK,
    ACCESS_MANAGEMENT,
    DATABASE,
    APPLICATION_SUPPORT,
    OTHER
};

// ---------------------------------------------------------------------------
// Conversion helpers (enum <-> string) used across the project and for
// persisting human-readable values into the SQLite database.
// ---------------------------------------------------------------------------

inline std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::EMPLOYEE: return "EMPLOYEE";
        case UserRole::ENGINEER: return "ENGINEER";
        case UserRole::ADMIN:    return "ADMIN";
    }
    return "UNKNOWN";
}

inline UserRole stringToRole(const std::string& s) {
    if (s == "EMPLOYEE") return UserRole::EMPLOYEE;
    if (s == "ENGINEER") return UserRole::ENGINEER;
    if (s == "ADMIN")    return UserRole::ADMIN;
    throw std::invalid_argument("Unknown role string: " + s);
}

inline std::string statusToString(TicketStatus status) {
    switch (status) {
        case TicketStatus::OPEN:        return "OPEN";
        case TicketStatus::ASSIGNED:    return "ASSIGNED";
        case TicketStatus::IN_PROGRESS: return "IN_PROGRESS";
        case TicketStatus::RESOLVED:    return "RESOLVED";
        case TicketStatus::CLOSED:      return "CLOSED";
    }
    return "UNKNOWN";
}

inline TicketStatus stringToStatus(const std::string& s) {
    if (s == "OPEN")        return TicketStatus::OPEN;
    if (s == "ASSIGNED")    return TicketStatus::ASSIGNED;
    if (s == "IN_PROGRESS") return TicketStatus::IN_PROGRESS;
    if (s == "RESOLVED")    return TicketStatus::RESOLVED;
    if (s == "CLOSED")      return TicketStatus::CLOSED;
    throw std::invalid_argument("Unknown status string: " + s);
}

inline std::string priorityToString(TicketPriority p) {
    switch (p) {
        case TicketPriority::LOW:      return "LOW";
        case TicketPriority::MEDIUM:   return "MEDIUM";
        case TicketPriority::HIGH:     return "HIGH";
        case TicketPriority::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

inline TicketPriority stringToPriority(const std::string& s) {
    if (s == "LOW")      return TicketPriority::LOW;
    if (s == "MEDIUM")   return TicketPriority::MEDIUM;
    if (s == "HIGH")     return TicketPriority::HIGH;
    if (s == "CRITICAL") return TicketPriority::CRITICAL;
    throw std::invalid_argument("Unknown priority string: " + s);
}

inline std::string ticketTypeToString(TicketType t) {
    switch (t) {
        case TicketType::SOFTWARE:             return "SOFTWARE";
        case TicketType::HARDWARE:             return "HARDWARE";
        case TicketType::INFOSEC:              return "INFOSEC";
        case TicketType::NETWORK:              return "NETWORK";
        case TicketType::ACCESS_MANAGEMENT:    return "ACCESS_MANAGEMENT";
        case TicketType::DATABASE:             return "DATABASE";
        case TicketType::APPLICATION_SUPPORT:  return "APPLICATION_SUPPORT";
        case TicketType::OTHER:                return "OTHER";
    }
    return "UNKNOWN";
}

inline TicketType stringToTicketType(const std::string& s) {
    if (s == "SOFTWARE")            return TicketType::SOFTWARE;
    if (s == "HARDWARE")            return TicketType::HARDWARE;
    if (s == "INFOSEC")             return TicketType::INFOSEC;
    if (s == "NETWORK")             return TicketType::NETWORK;
    if (s == "ACCESS_MANAGEMENT")   return TicketType::ACCESS_MANAGEMENT;
    if (s == "DATABASE")            return TicketType::DATABASE;
    if (s == "APPLICATION_SUPPORT") return TicketType::APPLICATION_SUPPORT;
    if (s == "OTHER")               return TicketType::OTHER;
    throw std::invalid_argument("Unknown ticket type string: " + s);
}

// Human-readable label used in menus (e.g. "Access Management" instead
// of the raw enum token "ACCESS_MANAGEMENT").
inline std::string ticketTypeToLabel(TicketType t) {
    switch (t) {
        case TicketType::SOFTWARE:             return "Software";
        case TicketType::HARDWARE:             return "Hardware";
        case TicketType::INFOSEC:              return "Information Security";
        case TicketType::NETWORK:              return "Network";
        case TicketType::ACCESS_MANAGEMENT:    return "Access Management";
        case TicketType::DATABASE:             return "Database";
        case TicketType::APPLICATION_SUPPORT:  return "Application Support";
        case TicketType::OTHER:                return "Other";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Custom exception hierarchy used throughout the project.
// ---------------------------------------------------------------------------

// Base exception for all application-level errors.
class AppException : public std::runtime_error {
public:
    explicit AppException(const std::string& message)
        : std::runtime_error(message) {}
};

// Thrown for any failure originating from the database layer
// (connection failures, failed statements, schema errors, etc.)
class DatabaseException : public AppException {
public:
    explicit DatabaseException(const std::string& message)
        : AppException("Database Error: " + message) {}
};

// Thrown when input validation fails (empty fields, invalid IDs, etc.)
class ValidationException : public AppException {
public:
    explicit ValidationException(const std::string& message)
        : AppException("Validation Error: " + message) {}
};

// Thrown when authentication (login) fails.
class AuthenticationException : public AppException {
public:
    explicit AuthenticationException(const std::string& message)
        : AppException("Authentication Error: " + message) {}
};

// Thrown when a requested record (user/ticket) cannot be found.
class NotFoundException : public AppException {
public:
    explicit NotFoundException(const std::string& message)
        : AppException("Not Found: " + message) {}
};

#endif // COMMON_H
