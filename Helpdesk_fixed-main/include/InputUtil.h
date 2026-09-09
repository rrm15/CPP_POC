#ifndef INPUT_UTIL_H
#define INPUT_UTIL_H

#include <string>
#include <vector>
#include <functional>

class InputUtil {
public:
    // Safe input reading
    static std::string readLine(const std::string& prompt = "", int max_length = 256);
    static int readInteger(const std::string& prompt = "", int min = INT_MIN, int max = INT_MAX);
    static std::string readOption(const std::vector<std::string>& options);
    
    // Input validation helpers
    static bool isNumeric(const std::string& str);
    static bool isAlphanumeric(const std::string& str);
    
private:
    InputUtil() = default;
};

#endif  // INPUT_UTIL_H
