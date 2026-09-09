#ifndef DATE_UTIL_H
#define DATE_UTIL_H

#include <chrono>
#include <string>

class DateUtil {
public:
    // Format time_point to readable string
    static std::string formatTime(const std::chrono::system_clock::time_point& tp,
                                 const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // Parse string to time_point
    static std::chrono::system_clock::time_point parseTime(const std::string& timeStr,
                                                          const std::string& format = "%Y-%m-%d %H:%M:%S");
    
    // Get current time
    static std::chrono::system_clock::time_point now();
    
private:
    DateUtil() = default;
};

#endif  // DATE_UTIL_H
