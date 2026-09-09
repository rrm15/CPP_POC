#ifndef USER_H
#define USER_H

#include "Common.h"
#include <string>

class UserManager {
public:
    UserManager();
    ~UserManager();
    
    // User management methods
    bool registerUser(const std::string& username, const std::string& email, 
                     const std::string& password, UserRole role);
    bool login(const std::string& username, const std::string& password);
    User* getCurrentUser() const;
    bool logout();
    
private:
    User* current_user;
    bool validatePassword(const std::string& password);
};

#endif  // USER_H
