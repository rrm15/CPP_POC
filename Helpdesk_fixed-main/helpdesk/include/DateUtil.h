#ifndef DATE_UTIL_H
#define DATE_UTIL_H

#include <string>

// Small helper namespace to avoid duplicating timestamp-generation logic
// across Ticket, DatabaseManager, CsvReportWriter, etc.
namespace DateUtil {

// Returns current local timestamp in "YYYY-MM-DD HH:MM:SS" format.
std::string nowString();

// Returns current local timestamp in "YYYYMMDD_HHMMSS" format suitable for filenames.
std::string nowFilenameTimestamp();

} // namespace DateUtil

#endif // DATE_UTIL_H
