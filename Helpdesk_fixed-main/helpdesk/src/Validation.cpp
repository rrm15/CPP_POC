#include "Validation.h"
#include "Common.h"
#include <cctype>
#include <sstream>

namespace {

bool isAllowedUsernameChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_';
}

// Splits `s` on '.' and returns false if any resulting label is empty
// (i.e. the string starts/ends with '.' or contains "..").
bool hasNoEmptyDotLabels(const std::string& s) {
    if (s.empty()) return false;
    if (s.front() == '.' || s.back() == '.') return false;
    std::stringstream ss(s);
    std::string label;
    while (std::getline(ss, label, '.')) {
        if (label.empty()) return false;
    }
    return true;
}

bool hasWhitespace(const std::string& s) {
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

} // namespace

namespace Validation {

bool isValidUsernameFormat(const std::string& username) {
    if (username.size() < 3 || username.size() > 32) return false;
    for (char c : username) {
        if (!isAllowedUsernameChar(c)) return false;
    }
    return true;
}

bool isValidEmailFormat(const std::string& email) {
    if (email.empty()) return false;
    if (hasWhitespace(email)) return false;

    // Exactly one '@'.
    size_t firstAt = email.find('@');
    if (firstAt == std::string::npos) return false;
    if (email.find('@', firstAt + 1) != std::string::npos) return false;

    std::string local = email.substr(0, firstAt);
    std::string domain = email.substr(firstAt + 1);

    if (local.empty() || domain.empty()) return false;
    if (!hasNoEmptyDotLabels(local)) return false;
    if (!hasNoEmptyDotLabels(domain)) return false;

    // Domain must contain at least one '.' (i.e. more than one label).
    if (domain.find('.') == std::string::npos) return false;

    return true;
}

void validateUsernameOrThrow(const std::string& username) {
    if (!isValidUsernameFormat(username)) {
        throw ValidationException(
            "Invalid username '" + username + "'. Usernames must be 3-32 characters "
            "long and may only contain letters, digits, '.' and '_'.");
    }
}

void validateEmailOrThrow(const std::string& email) {
    if (!isValidEmailFormat(email)) {
        throw ValidationException(
            "Invalid email address '" + email + "'. Expected format: "
            "local-part@domain.tld with no spaces.");
    }
}

} // namespace Validation
