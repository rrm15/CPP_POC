#include "DateUtil.h"
#include <ctime>
#include <sstream>
#include <iomanip>

namespace DateUtil {

std::string nowString() {
    std::time_t t = std::time(nullptr);
    std::tm tmStruct{};
#if defined(_WIN32) && !defined(__MINGW32__)
    localtime_s(&tmStruct, &t);
#else
    localtime_r(&t, &tmStruct);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmStruct, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string nowFilenameTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmStruct{};
#if defined(_WIN32) && !defined(__MINGW32__)
    localtime_s(&tmStruct, &t);
#else
    localtime_r(&t, &tmStruct);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmStruct, "%Y%m%d_%H%M%S");
    return oss.str();
}

} // namespace DateUtil
