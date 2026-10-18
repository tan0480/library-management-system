// Module 4: Report Generation
// Contributor: Lim Jun Yi (2506073)
// Purpose: Generates formatted library reports, statistics and text-file exports.

#ifndef REPORTING_H
#define REPORTING_H

#include "library_models.h"

#include <cstddef>
#include <iosfwd>
#include <string>

namespace lms {

// Returns the record counts used to calculate console report pages.
std::size_t countMemberReportRecords(const LibraryData& data);
std::size_t countInventoryReportRecords(const LibraryData& data);
std::size_t countBorrowingReportRecords(const LibraryData& data,
                                        const std::string& filter = "");
std::size_t countOverdueReportRecords(const LibraryData& data,
                                      const Date& asOf);
std::size_t countPaymentReportRecords(const LibraryData& data,
                                      const std::string& filter = "");
std::size_t countCategoryStatisticsRecords(const LibraryData& data);

// Writes one page of the formatted member account report.
void printMemberReport(const LibraryData& data, std::ostream& out,
                       std::size_t startingIndex = 0,
                       std::size_t maximumRecords = 50);
// Writes one page of inventory columns while totals still cover every title.
void printInventoryReport(const LibraryData& data, std::ostream& out,
                          std::size_t startingIndex = 0,
                          std::size_t maximumRecords = 50);
// Writes one page of filtered borrowing records, newest first.
void printBorrowingReport(const LibraryData& data, std::ostream& out,
                          const std::string& filter = "",
                          std::size_t startingIndex = 0,
                          std::size_t maximumRecords = 50);
// Writes one page of active loans overdue on the supplied report date.
void printOverdueReport(const LibraryData& data, const Date& asOf,
                        std::ostream& out, std::size_t startingIndex = 0,
                        std::size_t maximumRecords = 50);
// Writes one payment page and separates assessed fines from received cash.
void printPaymentReport(const LibraryData& data, std::ostream& out,
                        const std::string& filter = "",
                        std::size_t startingIndex = 0,
                        std::size_t maximumRecords = 50);
// Writes one page of the assignment's two-dimensional vector summary.
void printCategoryStatistics(const LibraryData& data, std::ostream& out,
                             std::size_t startingIndex = 0,
                             std::size_t maximumRecords = 50);
// Writes one complete selected report to its own text file.
bool exportSelectedReport(const LibraryData& data, int reportType,
                          const Date& reportDate,
                          const std::string& reportTitle,
                          const std::string& filter,
                          const std::string& fileName,
                          std::string& message);

}  // namespace lms

#endif
