#include "../include/Validation.h"

const std::regex Validation::EMAIL_PATTERN(
    "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"
);

const std::regex Validation::PASSWORD_PATTERN(
    "^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)(?=.*[@$!%*?&])[A-Za-z\\d@$!%*?&]{8,}$"
);

const std::regex Validation::USERNAME_PATTERN(
    "^[a-zA-Z0-9_]{3,20}$"
);

bool Validation::isValidEmail(const std::string& email) {
    return std::regex_match(email, EMAIL_PATTERN);
}

bool Validation::isValidPassword(const std::string& password) {
    return std::regex_match(password, PASSWORD_PATTERN);
}

bool Validation::isValidUsername(const std::string& username) {
    return std::regex_match(username, USERNAME_PATTERN);
}

bool Validation::isValidLength(const std::string& str, int min, int max) {
    return str.length() >= min && str.length() <= max;
}
