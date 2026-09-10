#ifndef INPUT_UTIL_H
#define INPUT_UTIL_H

#include <string>
#include "Common.h"

// Small collection of helpers for robust console input. Centralizing
// this logic avoids duplicated, error-prone cin-handling code across
// every menu flow, and keeps invalid-input handling consistent.
namespace InputUtil {

int readInt(const std::string& prompt);
std::string readLine(const std::string& prompt);
std::string readNonEmptyLine(const std::string& prompt);
TicketType readTicketType();
TicketPriority readPriority();

} // namespace InputUtil

#endif // INPUT_UTIL_H
