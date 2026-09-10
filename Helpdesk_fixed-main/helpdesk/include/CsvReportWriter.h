#ifndef CSV_REPORT_WRITER_H
#define CSV_REPORT_WRITER_H

#include <string>
#include <map>

// -----------------------------------------------------------------------
// OperationalMetrics
// -----------------------------------------------------------------------
// Authoritative snapshot of operational metrics calculated for the
// system administrator reports workflow.
// -----------------------------------------------------------------------
struct OperationalMetrics {
    std::string generatedAt;
    int totalTickets = 0;
    std::map<std::string, int> statusCounts;
    std::map<std::string, int> priorityCounts;
    double averageRating = 0.0;
};

// -----------------------------------------------------------------------
// CsvReportWriter
// -----------------------------------------------------------------------
// Focused, pure CSV writer for operational metrics reporting.
// Implements RFC 4180 escaping and collision-safe file generation.
// Has no dependencies on SQLite or terminal I/O.
// -----------------------------------------------------------------------
class CsvReportWriter {
public:
    // Escapes a single string field according to RFC 4180 rules:
    // Quotes fields containing commas, double quotes, CR (\r), or LF (\n).
    // Doubles embedded double quotes (" -> "").
    static std::string escapeField(const std::string& field);

    // Resolves a unique filename under outputDir avoiding same-second overwrites:
    // operational_metrics_YYYYMMDD_HHMMSS.csv, _1.csv, _2.csv, etc.
    static std::string resolveUniqueFilename(const std::string& outputDir, const std::string& timestamp);

    // Writes the operational metrics snapshot to a CSV file.
    // Checks directory creation, file opening, writing, flushing, and final stream state.
    // Returns the path of the successfully written file.
    // Throws AppException on any file I/O failure.
    static std::string writeOperationalMetrics(
        const OperationalMetrics& metrics,
        const std::string& outputDir = "build/reports/admin"
    );
};

#endif // CSV_REPORT_WRITER_H
