#ifndef DATE_UTIL_H
#define DATE_UTIL_H

#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>

// Small helper namespace to avoid duplicating timestamp-generation logic
// across Ticket, DatabaseManager, etc.
namespace DateUtil {

inline std::string nowString() {
    std::time_t t = std::time(nullptr);
    std::tm tmStruct{};
#if defined(_WIN32)
    localtime_s(&tmStruct, &t);
#else
    localtime_r(&t, &tmStruct);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmStruct, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace DateUtil

#endif // DATE_UTIL_H
