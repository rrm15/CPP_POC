#ifndef INPUT_UTIL_H
#define INPUT_UTIL_H

#include <iostream>
#include <string>
#include <limits>
#include "Common.h"

// Small collection of helpers for robust console input. Centralizing
// this logic avoids duplicated, error-prone cin-handling code across
// every menu flow, and keeps invalid-input handling consistent.
namespace InputUtil {

inline int readInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        try {
            size_t pos;
            int value = std::stoi(line, &pos);
            if (pos != line.size()) throw std::invalid_argument("trailing characters");
            return value;
        } catch (const std::exception&) {
            std::cout << "Invalid input. Please enter a whole number.\n";
        }
    }
}

inline std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

inline std::string readNonEmptyLine(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt);
        if (!line.empty()) return line;
        std::cout << "This field cannot be empty. Please try again.\n";
    }
}

inline TicketType readTicketType() {
    std::cout << "Select Ticket Type:\n"
              << "  1. Software\n"
              << "  2. Hardware\n"
              << "  3. Information Security\n"
              << "  4. Network\n"
              << "  5. Access Management\n"
              << "  6. Database\n"
              << "  7. Application Support\n"
              << "  8. Other\n";
    while (true) {
        int choice = readInt("Choice: ");
        switch (choice) {
            case 1: return TicketType::SOFTWARE;
            case 2: return TicketType::HARDWARE;
            case 3: return TicketType::INFOSEC;
            case 4: return TicketType::NETWORK;
            case 5: return TicketType::ACCESS_MANAGEMENT;
            case 6: return TicketType::DATABASE;
            case 7: return TicketType::APPLICATION_SUPPORT;
            case 8: return TicketType::OTHER;
            default: std::cout << "Invalid choice, try again.\n";
        }
    }
}

inline TicketPriority readPriority() {
    std::cout << "Select priority:\n"
              << "  1. LOW\n  2. MEDIUM\n  3. HIGH\n  4. CRITICAL\n";
    while (true) {
        int choice = readInt("Choice: ");
        switch (choice) {
            case 1: return TicketPriority::LOW;
            case 2: return TicketPriority::MEDIUM;
            case 3: return TicketPriority::HIGH;
            case 4: return TicketPriority::CRITICAL;
            default: std::cout << "Invalid choice, try again.\n";
        }
    }
}

} // namespace InputUtil

#endif // INPUT_UTIL_H
