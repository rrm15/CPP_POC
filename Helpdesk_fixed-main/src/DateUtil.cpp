#include "../include/DateUtil.h"
#include <iomanip>
#include <sstream>
#include <ctime>

std::string DateUtil::formatTime(const std::chrono::system_clock::time_point& tp,
                                const std::string& format) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&tt);
    
    std::stringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str();
}

std::chrono::system_clock::time_point DateUtil::parseTime(const std::string& timeStr,
                                                         const std::string& format) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, format.c_str());
    
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return tp;
}

std::chrono::system_clock::time_point DateUtil::now() {
    return std::chrono::system_clock::now();
}
