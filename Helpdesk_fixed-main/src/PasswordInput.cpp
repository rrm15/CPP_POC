#include "../include/PasswordInput.h"
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <cstring>

std::string PasswordInput::readPassword(const std::string& prompt) {
    std::cout << prompt << std::flush;
    
    struct termios old_settings, new_settings;
    tcgetattr(STDIN_FILENO, &old_settings);
    new_settings = old_settings;
    new_settings.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    
    std::string password;
    std::getline(std::cin, password);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
    std::cout << std::endl;
    
    return password;
}

bool PasswordInput::verifyPasswordMatch(const std::string& password1,
                                       const std::string& password2) {
    return password1 == password2;
}
