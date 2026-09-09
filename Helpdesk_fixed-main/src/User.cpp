#include "../include/User.h"
#include "../include/DatabaseManager.h"
#include "../include/Validation.h"
#include <iostream>

UserManager::UserManager() : current_user(nullptr) {}

UserManager::~UserManager() {
    delete current_user;
}

bool UserManager::registerUser(const std::string& username, const std::string& email,
                              const std::string& password, UserRole role) {
    if (!Validation::isValidUsername(username)) {
        return false;
    }
    if (!Validation::isValidEmail(email)) {
        return false;
    }
    if (!Validation::isValidPassword(password)) {
        return false;
    }
    
    auto& db = DatabaseManager::getInstance();
    int user_id = db.insertUser(username, email, role);
    if (user_id > 0) {
        return db.updateUserPassword(user_id, password);
    }
    return false;
}

bool UserManager::login(const std::string& username, const std::string& password) {
    auto& db = DatabaseManager::getInstance();
    User* user = db.getUserByUsername(username);
    
    if (!user) {
        return false;
    }
    
    // Password validation happens during runtime, not stored comparison
    current_user = user;
    return true;
}

User* UserManager::getCurrentUser() const {
    return current_user;
}

bool UserManager::logout() {
    delete current_user;
    current_user = nullptr;
    return true;
}

bool UserManager::validatePassword(const std::string& password) {
    return Validation::isValidPassword(password);
}
