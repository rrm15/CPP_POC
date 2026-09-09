#ifndef VALIDATION_H
#define VALIDATION_H

#include <string>
#include <regex>

class Validation {
public:
    // Email validation
    static bool isValidEmail(const std::string& email);
    
    // Password validation (min 8 chars, uppercase, digit, special)
    static bool isValidPassword(const std::string& password);
    
    // Username validation (alphanumeric + underscore, 3-20 chars)
    static bool isValidUsername(const std::string& username);
    
    // String length validation
    static bool isValidLength(const std::string& str, int min, int max);
    
private:
    static const std::regex EMAIL_PATTERN;
    static const std::regex PASSWORD_PATTERN;
    static const std::regex USERNAME_PATTERN;
};

#endif  // VALIDATION_H
