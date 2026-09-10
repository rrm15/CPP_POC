#include "DateUtil.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>

namespace DateUtil {

std::string nowString() {
    static std::mutex mtx;
    static auto lastTime = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(mtx);

    auto now = std::chrono::system_clock::now();
    if (now <= lastTime) {
        now = lastTime + std::chrono::milliseconds(1);
    }
    lastTime = now;

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmStruct{};
#if defined(_WIN32) && !defined(__MINGW32__)
    localtime_s(&tmStruct, &t);
#else
    localtime_r(&t, &tmStruct);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmStruct, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
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
