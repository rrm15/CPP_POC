#include "CsvReportWriter.h"
#include "Common.h"
#include "DateUtil.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

std::string CsvReportWriter::escapeField(const std::string& field) {
    bool needsQuotes = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\r' || c == '\n') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return field;
    }

    std::string escaped;
    escaped.reserve(field.size() + 10);
    escaped.push_back('"');
    for (char c : field) {
        if (c == '"') {
            escaped.push_back('"');
            escaped.push_back('"');
        } else {
            escaped.push_back(c);
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string CsvReportWriter::resolveUniqueFilename(const std::string& outputDir, const std::string& timestamp) {
    std::string cleanTs;
    for (char c : timestamp) {
        if ((c >= '0' && c <= '9') || c == '_') {
            // Keep digits and underscores (YYYYMMDD_HHMMSS already safe)
            cleanTs.push_back(c);
        } else if (c == ' ') {
            // Space between date and time parts -> underscore separator
            if (!cleanTs.empty() && cleanTs.back() != '_') {
                cleanTs.push_back('_');
            }
        }
        // Strip everything else (dashes, colons, etc.) — digits already captured
    }
    if (cleanTs.empty()) {
        cleanTs = DateUtil::nowFilenameTimestamp();
    }

    std::string basePath = (fs::path(outputDir) / ("operational_metrics_" + cleanTs)).string();
    std::string candidate = basePath + ".csv";
    if (!fs::exists(candidate)) {
        return candidate;
    }

    int suffix = 1;
    while (true) {
        std::string candidateSuffix = basePath + "_" + std::to_string(suffix) + ".csv";
        if (!fs::exists(candidateSuffix)) {
            return candidateSuffix;
        }
        ++suffix;
    }
}

std::string CsvReportWriter::writeOperationalMetrics(
    const OperationalMetrics& metrics,
    const std::string& outputDir
) {
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        throw AppException("Failed to create report directory '" + outputDir + "': " + ec.message());
    }

    std::string filePath = resolveUniqueFilename(outputDir, metrics.generatedAt);
    std::ofstream out(filePath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        throw AppException("Failed to open report file for writing: " + filePath);
    }

    // Header row
    out << "generated_at,total_tickets,"
        << "status_open,status_assigned,status_in_progress,status_resolved,status_closed,"
        << "priority_low,priority_medium,priority_high,priority_critical,"
        << "average_feedback_rating\n";

    auto getCount = [](const std::map<std::string, int>& m, const std::string& key) -> int {
        auto it = m.find(key);
        return (it != m.end()) ? it->second : 0;
    };

    int openCount       = getCount(metrics.statusCounts, "OPEN");
    int assignedCount   = getCount(metrics.statusCounts, "ASSIGNED");
    int inProgressCount = getCount(metrics.statusCounts, "IN_PROGRESS");
    int resolvedCount   = getCount(metrics.statusCounts, "RESOLVED");
    int closedCount     = getCount(metrics.statusCounts, "CLOSED");

    int lowCount      = getCount(metrics.priorityCounts, "LOW");
    int mediumCount   = getCount(metrics.priorityCounts, "MEDIUM");
    int highCount     = getCount(metrics.priorityCounts, "HIGH");
    int criticalCount = getCount(metrics.priorityCounts, "CRITICAL");

    std::ostringstream ratingStream;
    ratingStream << std::fixed << std::setprecision(2) << metrics.averageRating;

    out << escapeField(metrics.generatedAt) << ","
        << metrics.totalTickets << ","
        << openCount << ","
        << assignedCount << ","
        << inProgressCount << ","
        << resolvedCount << ","
        << closedCount << ","
        << lowCount << ","
        << mediumCount << ","
        << highCount << ","
        << criticalCount << ","
        << ratingStream.str() << "\n";

    out.flush();
    if (!out.good()) {
        out.close();
        throw AppException("Failed writing metrics content to file: " + filePath);
    }
    out.close();
    return filePath;
}
