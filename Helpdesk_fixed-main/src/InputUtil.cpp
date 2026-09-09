#include "../include/InputUtil.h"
#include <iostream>
#include <algorithm>

std::string InputUtil::readLine(const std::string& prompt, int max_length) {
    if (!prompt.empty()) {
        std::cout << prompt << std::flush;
    }
    
    std::string input;
    std::getline(std::cin, input);
    
    if (input.length() > max_length) {
        input = input.substr(0, max_length);
    }
    
    return input;
}

int InputUtil::readInteger(const std::string& prompt, int min, int max) {
    int value;
    while (true) {
        if (!prompt.empty()) {
            std::cout << prompt << std::flush;
        }
        
        if (std::cin >> value) {
            if (value >= min && value <= max) {
                std::cin.ignore(10000, '\n');
                return value;
            }
        }
        
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "Invalid input. Please try again." << std::endl;
    }
}

std::string InputUtil::readOption(const std::vector<std::string>& options) {
    for (size_t i = 0; i < options.size(); ++i) {
        std::cout << (i + 1) << ". " << options[i] << std::endl;
    }
    
    int choice = readInteger("Select option: ", 1, options.size());
    return options[choice - 1];
}

bool InputUtil::isNumeric(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

bool InputUtil::isAlphanumeric(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isalnum);
}
