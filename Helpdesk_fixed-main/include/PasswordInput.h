#ifndef PASSWORD_INPUT_H
#define PASSWORD_INPUT_H

#include <string>

class PasswordInput {
public:
    // Read password from stdin without echo
    static std::string readPassword(const std::string& prompt = "Password: ");
    
    // Verify password matches
    static bool verifyPasswordMatch(const std::string& password1, 
                                   const std::string& password2);
    
private:
    PasswordInput() = default;
};

#endif  // PASSWORD_INPUT_H
