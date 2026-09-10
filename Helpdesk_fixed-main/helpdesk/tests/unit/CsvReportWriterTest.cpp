#include <gtest/gtest.h>
#include "CsvReportWriter.h"
#include "Common.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// Helper: read entire file content into a string
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// -----------------------------------------------------------------------
// escapeField
// -----------------------------------------------------------------------
TEST(CsvReportWriterEscapeTest, PlainStringUnchanged) {
    EXPECT_EQ(CsvReportWriter::escapeField("hello"), "hello");
}

TEST(CsvReportWriterEscapeTest, EmptyStringUnchanged) {
    EXPECT_EQ(CsvReportWriter::escapeField(""), "");
}

TEST(CsvReportWriterEscapeTest, StringWithCommaIsQuoted) {
    std::string result = CsvReportWriter::escapeField("a,b");
    EXPECT_EQ(result, "\"a,b\"");
}

TEST(CsvReportWriterEscapeTest, StringWithDoubleQuoteEscapes) {
    std::string result = CsvReportWriter::escapeField("say \"hi\"");
    EXPECT_EQ(result, "\"say \"\"hi\"\"\"");
}

TEST(CsvReportWriterEscapeTest, StringWithNewlineIsQuoted) {
    std::string result = CsvReportWriter::escapeField("line1\nline2");
    EXPECT_TRUE(result.front() == '"' && result.back() == '"');
}

TEST(CsvReportWriterEscapeTest, StringWithCRIsQuoted) {
    std::string result = CsvReportWriter::escapeField("cr\r");
    EXPECT_TRUE(result.front() == '"' && result.back() == '"');
}

// -----------------------------------------------------------------------
// resolveUniqueFilename - collision avoidance
// -----------------------------------------------------------------------
class CsvReportWriterFileTest : public ::testing::Test {
protected:
    std::string tmpDir;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tmpDir = std::string("test_csv_") + info->test_suite_name() + "_" + info->name();
        for (char& c : tmpDir) {
            if (c == '/' || c == '\\') c = '_';
        }
        fs::create_directories(tmpDir);
    }

    void TearDown() override {
        fs::remove_all(tmpDir);
    }
};

TEST_F(CsvReportWriterFileTest, NoCollisionReturnsBaseFilename) {
    std::string path = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    EXPECT_TRUE(path.find("operational_metrics_20260101_120000.csv") != std::string::npos);
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(CsvReportWriterFileTest, CollisionAddsSuffix) {
    std::string base = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    // Create the base file so it already exists
    { std::ofstream touch(base); touch << "x"; }

    std::string second = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    EXPECT_TRUE(second.find("_1.csv") != std::string::npos);
    EXPECT_FALSE(fs::exists(second));
}

TEST_F(CsvReportWriterFileTest, MultipleCollisionsIncrementSuffix) {
    std::string base = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    { std::ofstream touch(base); touch << "x"; }
    std::string s1 = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    { std::ofstream touch(s1); touch << "x"; }
    std::string s2 = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    EXPECT_TRUE(s2.find("_2.csv") != std::string::npos);
}

// -----------------------------------------------------------------------
// writeOperationalMetrics - file content
// -----------------------------------------------------------------------
TEST_F(CsvReportWriterFileTest, WritesHeaderAndDataRow) {
    OperationalMetrics m;
    m.generatedAt = "2026-01-01 12:00:00";
    m.totalTickets = 5;
    m.statusCounts["OPEN"] = 3;
    m.statusCounts["RESOLVED"] = 2;
    m.priorityCounts["HIGH"] = 2;
    m.priorityCounts["LOW"] = 3;
    m.averageRating = 4.5;

    std::string path = CsvReportWriter::writeOperationalMetrics(m, tmpDir);
    ASSERT_TRUE(fs::exists(path));

    std::string content = readFile(path);
    EXPECT_TRUE(content.find("generated_at,total_tickets") != std::string::npos);
    EXPECT_TRUE(content.find("2026-01-01 12:00:00") != std::string::npos);
    EXPECT_TRUE(content.find(",5,") != std::string::npos);
    EXPECT_TRUE(content.find("4.50") != std::string::npos);
}

TEST_F(CsvReportWriterFileTest, ThrowsIfDirectoryCreationFails) {
    // Use a path rooted at a non-writable location to provoke failure
    // (use the base test file path as a "directory" - it's a file, not a dir)
    std::string base = CsvReportWriter::resolveUniqueFilename(tmpDir, "20260101_120000");
    { std::ofstream touch(base); touch << "x"; }

    OperationalMetrics m;
    m.generatedAt = "2026-01-01 12:00:00";
    // Writing into a path using the file itself as a directory component
    // triggers a filesystem error on fs::create_directories
    EXPECT_THROW(
        CsvReportWriter::writeOperationalMetrics(m, base + "/subdir"),
        AppException
    );
}

TEST_F(CsvReportWriterFileTest, EscapedTimestampAppearsProperly) {
    OperationalMetrics m;
    m.generatedAt = "2026-01-01 12:00:00"; // no special chars - should not be quoted
    m.totalTickets = 0;
    m.averageRating = 0.0;

    std::string path = CsvReportWriter::writeOperationalMetrics(m, tmpDir);
    std::string content = readFile(path);
    // generatedAt has no comma/quote so should appear raw
    EXPECT_TRUE(content.find("2026-01-01 12:00:00") != std::string::npos);
}
