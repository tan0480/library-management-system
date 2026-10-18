// Module 4: Report Generation
// Contributor: Lim Jun Yi (2506073)
// Purpose: Generates formatted library reports, statistics and text-file exports.

#include "reporting.h"

#include "additional_functionalities.h"
#include "book_borrowing.h"
#include "date_functions.h"
#include "payment_functions.h"
#include "record_search.h"
#include "string_functions.h"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

using namespace std;

namespace lms {
namespace {

std::string shortened(const std::string& text, std::size_t width) {
    if (text.size() <= width) {
        return text;
    }
    if (width <= 3) {
        return text.substr(0, width);
    }
    return text.substr(0, width - 3) + "...";
}

std::string loanStatusText(LoanStatus status) {
    return status == LoanStatus::Active ? "ACTIVE" : "RETURNED";
}

bool containsFilter(const std::string& text, const std::string& filter) {
    return lowerCopy(text).find(filter) != std::string::npos;
}

bool loanMatchesFilter(const LibraryData& data, const Loan& loan,
                       const std::string& filter) {
    // A loan matches when the keyword appears in any useful report field.
    return filter.empty() || containsFilter(std::to_string(loan.id), filter) ||
           containsFilter(loan.memberId, filter) ||
           containsFilter(memberName(data, loan.memberId), filter) ||
           containsFilter(loan.bookId, filter) ||
           containsFilter(bookTitle(data, loan.bookId), filter) ||
           containsFilter(loanStatusText(loan.status), filter);
}

bool paymentMatchesFilter(const LibraryData& data, const Payment& payment,
                          const std::string& filter) {
    // Payment filters cover IDs, member details, type, date and description.
    return filter.empty() ||
           containsFilter(std::to_string(payment.id), filter) ||
           containsFilter(payment.memberId, filter) ||
           containsFilter(memberName(data, payment.memberId), filter) ||
           containsFilter(payment.type, filter) ||
           containsFilter(paymentMethodLabel(payment.method), filter) ||
           containsFilter(formatDate(payment.date), filter) ||
           containsFilter(payment.note, filter);
}

std::vector<const Loan*> sortedMatchingLoans(const LibraryData& data,
                                             const std::string& filter) {
    std::vector<const Loan*> loans;
    for (const Loan& loan : data.loans) {
        if (loanMatchesFilter(data, loan, filter)) {
            loans.push_back(&loan);
        }
    }
    // Selection sort uses the actual date, then the larger ID for equal dates.
    for (std::size_t position = 0; position < loans.size(); ++position) {
        std::size_t newestPosition = position;
        for (std::size_t candidate = position + 1; candidate < loans.size();
             ++candidate) {
            const long candidateDate = dateToSerial(loans[candidate]->borrowDate);
            const long newestDate =
                dateToSerial(loans[newestPosition]->borrowDate);
            if (candidateDate > newestDate ||
                (candidateDate == newestDate &&
                 loans[candidate]->id > loans[newestPosition]->id)) {
                newestPosition = candidate;
            }
        }
        if (newestPosition != position) {
            const Loan* temporary = loans[position];
            loans[position] = loans[newestPosition];
            loans[newestPosition] = temporary;
        }
    }
    return loans;
}

std::vector<const Payment*> sortedMatchingPayments(
    const LibraryData& data, const std::string& filter) {
    std::vector<const Payment*> payments;
    for (const Payment& payment : data.payments) {
        if (paymentMatchesFilter(data, payment, filter)) {
            payments.push_back(&payment);
        }
    }
    for (std::size_t position = 0; position < payments.size(); ++position) {
        std::size_t newestPosition = position;
        for (std::size_t candidate = position + 1; candidate < payments.size();
             ++candidate) {
            const long candidateDate = dateToSerial(payments[candidate]->date);
            const long newestDate = dateToSerial(payments[newestPosition]->date);
            if (candidateDate > newestDate ||
                (candidateDate == newestDate &&
                 payments[candidate]->id > payments[newestPosition]->id)) {
                newestPosition = candidate;
            }
        }
        if (newestPosition != position) {
            const Payment* temporary = payments[position];
            payments[position] = payments[newestPosition];
            payments[newestPosition] = temporary;
        }
    }
    return payments;
}

std::vector<std::string> categoryNames(const LibraryData& data) {
    std::vector<std::string> categories;
    for (const Book& book : data.books) {
        if (!book.active) {
            continue;
        }
        bool categoryExists = false;
        for (std::size_t index = 0; index < categories.size(); ++index) {
            if (lowerCopy(categories[index]) == lowerCopy(book.category)) {
                categoryExists = true;
                break;
            }
        }
        if (!categoryExists) {
            categories.push_back(book.category);
        }
    }
    return categories;
}

std::string reportTypeText(int reportType) {
    switch (reportType) {
        case 1:
            return "Member Account Report";
        case 2:
            return "Book Inventory Report";
        case 3:
            return "Borrowing and Return Report";
        case 4:
            return "Overdue Report";
        case 5:
            return "Payment Transaction Report";
        case 6:
            return "Category Statistics";
    }
    return "Unknown Report";
}

std::string generatedDateAndTime() {
    const std::time_t currentTime = std::time(nullptr);
    const std::tm* localTime = std::localtime(&currentTime);
    if (localTime == nullptr) {
        return "Unavailable";
    }
    char formattedTime[20] = {};
    if (std::strftime(formattedTime, sizeof(formattedTime),
                      "%Y-%m-%d %H:%M:%S", localTime) == 0) {
        return "Unavailable";
    }
    return formattedTime;
}

}  // namespace

std::size_t countMemberReportRecords(const LibraryData& data) {
    std::size_t count = 0;
    for (const Member& member : data.members) {
        if (member.active) {
            ++count;
        }
    }
    return count;
}

std::size_t countInventoryReportRecords(const LibraryData& data) {
    std::size_t count = 0;
    for (const Book& book : data.books) {
        if (book.active) {
            ++count;
        }
    }
    return count;
}

std::size_t countBorrowingReportRecords(const LibraryData& data,
                                        const std::string& filter) {
    const std::string cleanedFilter = lowerCopy(trim(filter));
    std::size_t count = 0;
    for (const Loan& loan : data.loans) {
        if (loanMatchesFilter(data, loan, cleanedFilter)) {
            ++count;
        }
    }
    return count;
}

std::size_t countOverdueReportRecords(const LibraryData& data,
                                      const Date& asOf) {
    std::size_t count = 0;
    for (const Loan& loan : data.loans) {
        if (loan.status == LoanStatus::Active &&
            daysBetween(loan.dueDate, asOf) > 0) {
            ++count;
        }
    }
    return count;
}

std::size_t countPaymentReportRecords(const LibraryData& data,
                                      const std::string& filter) {
    const std::string cleanedFilter = lowerCopy(trim(filter));
    std::size_t count = 0;
    for (const Payment& payment : data.payments) {
        if (paymentMatchesFilter(data, payment, cleanedFilter)) {
            ++count;
        }
    }
    return count;
}

std::size_t countCategoryStatisticsRecords(const LibraryData& data) {
    return categoryNames(data).size();
}

void printMemberReport(const LibraryData& data, std::ostream& out,
                       std::size_t startingIndex,
                       std::size_t maximumRecords) {
    out << "\nMEMBER ACCOUNT REPORT\n"
        << std::string(92, '=') << '\n'
        << std::left << std::setw(10) << "ID" << std::setw(24) << "Name"
        << std::setw(17) << "Phone" << std::setw(27) << "Email"
        << std::right << std::setw(7) << "Loans" << std::setw(7) << "Fine"
        << '\n' << std::string(92, '-') << '\n';

    // Active-loan and fine totals come from the same shared LibraryData records.
    std::size_t activeMembers = 0;
    std::size_t displayedMembers = 0;
    for (const Member& member : data.members) {
        if (!member.active) {
            continue;
        }
        const std::size_t memberIndex = activeMembers;
        ++activeMembers;
        if (memberIndex >= startingIndex &&
            displayedMembers < maximumRecords) {
            out << std::left << std::setw(10) << member.id << std::setw(24)
                << shortened(member.name, 23) << std::setw(17)
                << shortened(member.phone, 16) << std::setw(27)
                << shortened(member.email, 26) << std::right << std::setw(7)
                << countActiveLoans(data, member.id) << std::setw(7)
                << std::fixed << std::setprecision(2)
                << member.outstandingFine << '\n';
            ++displayedMembers;
        }
    }
    if (activeMembers == 0) {
        out << "No records found.\n";
    }
    out << std::string(92, '-') << "\nShowing " << displayedMembers << " of "
        << activeMembers << " active members.\n";
}

void printInventoryReport(const LibraryData& data, std::ostream& out,
                          std::size_t startingIndex,
                          std::size_t maximumRecords) {
    out << "\nBOOK INVENTORY REPORT\n"
        << std::string(108, '=') << '\n'
        << std::left << std::setw(9) << "ID" << std::setw(31) << "Title"
        << std::setw(22) << "Author" << std::setw(18) << "Category"
        << std::right << std::setw(6) << "Total" << std::setw(7) << "Avail"
        << std::setw(9) << "Borrowed" << std::setw(6) << "Held" << '\n'
        << std::string(108, '-') << '\n';

    std::size_t totalTitles = 0;
    std::size_t displayedTitles = 0;
    int totalCopies = 0;
    int totalAvailable = 0;
    int totalBorrowed = 0;
    std::size_t bookIndex = 0;
    while (bookIndex < data.books.size()) {
        const Book& book = data.books[bookIndex];
        ++bookIndex;
        if (!book.active) {
            continue;
        }
        const std::size_t activeBookIndex = totalTitles;
        ++totalTitles;
        totalCopies += book.totalCopies;
        totalAvailable += book.availableCopies;
        // Borrowed copies are the difference between total and physical stock.
        const int borrowedCopies = book.totalCopies - book.availableCopies;
        totalBorrowed += borrowedCopies;
        if (activeBookIndex >= startingIndex &&
            displayedTitles < maximumRecords) {
            out << std::left << std::setw(9) << book.id << std::setw(31)
                << shortened(book.title, 30) << std::setw(22)
                << shortened(book.author, 21) << std::setw(18)
                << shortened(book.category, 17) << std::right << std::setw(6)
                << book.totalCopies << std::setw(7) << book.availableCopies
                << std::setw(9) << borrowedCopies << std::setw(6)
                << heldCopiesForReservations(data, book.id) << '\n';
            ++displayedTitles;
        }
    }
    if (totalTitles == 0) {
        out << "No records found.\n";
    }
    out << std::string(108, '-') << "\nShowing " << displayedTitles << " of "
        << totalTitles << " active titles.\nTitles: " << totalTitles
        << " | Total copies: " << totalCopies
        << " | Available: " << totalAvailable
        << " | Borrowed: " << totalBorrowed << "\n";
}

void printBorrowingReport(const LibraryData& data, std::ostream& out,
                          const std::string& filter,
                          std::size_t startingIndex,
                          std::size_t maximumRecords) {
    out << "\nBORROWING AND RETURN REPORT\n"
        << std::string(103, '=') << '\n'
        << std::left << std::setw(7) << "Loan" << std::setw(11) << "Member"
        << std::setw(28) << "Book" << std::setw(12) << "Borrowed"
        << std::setw(12) << "Due" << std::setw(12) << "Returned"
        << std::setw(11) << "Status" << std::right << std::setw(10) << "Fine"
        << '\n' << std::string(103, '-') << '\n';

    const std::string cleanedFilter = lowerCopy(trim(filter));
    const std::vector<const Loan*> matchingLoans =
        sortedMatchingLoans(data, cleanedFilter);
    const std::size_t matchingRecords = matchingLoans.size();
    std::size_t displayedRecords = 0;
    std::size_t loanIndex = startingIndex;
    while (loanIndex < matchingLoans.size() &&
           displayedRecords < maximumRecords) {
        const Loan* loan = matchingLoans[loanIndex];
        out << std::left << std::setw(7) << loan->id << std::setw(11)
            << loan->memberId << std::setw(28)
            << shortened(bookTitle(data, loan->bookId), 27) << std::setw(12)
            << formatDate(loan->borrowDate) << std::setw(12)
            << formatDate(loan->dueDate) << std::setw(12)
            << formatDate(loan->returnDate) << std::setw(11)
            << loanStatusText(loan->status) << std::right << std::setw(10)
            << std::fixed << std::setprecision(2) << loan->fineCharged << '\n';
        ++loanIndex;
        ++displayedRecords;
    }
    if (matchingRecords == 0) {
        out << "No records found.\n";
    }
    out << std::string(103, '-') << "\nShowing " << displayedRecords << " of "
        << matchingRecords << " matching records (newest first).\n";
}

void printOverdueReport(const LibraryData& data, const Date& asOf,
                        std::ostream& out, std::size_t startingIndex,
                        std::size_t maximumRecords) {
    out << "\nOVERDUE REPORT AS OF " << formatDate(asOf) << '\n'
        << std::string(83, '=') << '\n'
        << std::left << std::setw(7) << "Loan" << std::setw(22) << "Member"
        << std::setw(30) << "Book" << std::setw(12) << "Due"
        << std::right << std::setw(6) << "Days" << std::setw(10) << "Fine"
        << '\n' << std::string(83, '-') << '\n';

    // Projected fines use active loans only and the supplied report date.
    std::size_t overdueCount = 0;
    std::size_t displayedRecords = 0;
    double projectedFines = 0.0;
    for (const Loan& loan : data.loans) {
        const int overdueDays = daysBetween(loan.dueDate, asOf);
        if (loan.status != LoanStatus::Active || overdueDays <= 0) {
            continue;
        }
        const double fine = overdueDays * OVERDUE_FINE_PER_DAY;
        projectedFines += fine;
        const std::size_t overdueIndex = overdueCount;
        ++overdueCount;
        if (overdueIndex >= startingIndex &&
            displayedRecords < maximumRecords) {
            out << std::left << std::setw(7) << loan.id << std::setw(22)
                << shortened(memberName(data, loan.memberId), 21)
                << std::setw(30) << shortened(bookTitle(data, loan.bookId), 29)
                << std::setw(12) << formatDate(loan.dueDate) << std::right
                << std::setw(6) << overdueDays << std::setw(10) << std::fixed
                << std::setprecision(2) << fine << '\n';
            ++displayedRecords;
        }
    }
    if (overdueCount == 0) {
        out << "No records found.\n";
    }
    out << std::string(83, '-') << "\nShowing " << displayedRecords << " of "
        << overdueCount << " overdue loans.\nOverdue loans: " << overdueCount
        << " | Projected fines: RM" << std::fixed << std::setprecision(2)
        << projectedFines << "\n";
}

void printPaymentReport(const LibraryData& data, std::ostream& out,
                        const std::string& filter,
                        std::size_t startingIndex,
                        std::size_t maximumRecords) {
    out << "\nPAYMENT TRANSACTION REPORT\n"
        << std::string(108, '=') << '\n'
        << std::left << std::setw(8) << "Txn" << std::setw(11) << "Member"
        << std::setw(20) << "Type" << std::setw(17) << "Method"
        << std::setw(12) << "Date"
        << std::setw(9) << "Ref" << std::right << std::setw(11) << "Amount"
        << "  Note\n" << std::string(108, '-') << '\n';

    // Totals cover all matching payments, including records beyond the display limit.
    const std::string cleanedFilter = lowerCopy(trim(filter));
    double fineChargesAssessed = 0.0;
    double netCashMovement = 0.0;
    const std::vector<const Payment*> matchingPayments =
        sortedMatchingPayments(data, cleanedFilter);
    const std::size_t matchingTransactions = matchingPayments.size();
    for (const Payment* payment : matchingPayments) {
        // FINE_CHARGE creates an amount owed, so it must not count as received cash.
        if (payment->type == "FINE_CHARGE") {
            fineChargesAssessed += payment->amount;
        } else {
            netCashMovement += payment->amount;
        }
    }

    std::size_t displayedTransactions = 0;
    std::size_t paymentIndex = startingIndex;
    while (paymentIndex < matchingPayments.size() &&
           displayedTransactions < maximumRecords) {
        const Payment* payment = matchingPayments[paymentIndex];
        out << std::left << std::setw(8) << payment->id << std::setw(11)
            << payment->memberId << std::setw(20) << payment->type
            << std::setw(17) << paymentMethodLabel(payment->method)
            << std::setw(12) << formatDate(payment->date) << std::setw(9)
            << payment->referenceId
            << std::right << std::setw(11) << std::fixed << std::setprecision(2)
            << payment->amount << "  " << shortened(payment->note, 35) << '\n';
        ++paymentIndex;
        ++displayedTransactions;
    }
    if (matchingTransactions == 0) {
        out << "No records found.\n";
    }
    out << std::string(108, '-') << "\nShowing " << displayedTransactions
        << " of " << matchingTransactions << " matching transactions.\n"
        << "Fine charges assessed (not cash): RM" << std::fixed
        << std::setprecision(2) << fineChargesAssessed
        << " | Net cash movement: RM" << netCashMovement << "\n";
}

void printCategoryStatistics(const LibraryData& data, std::ostream& out,
                             std::size_t startingIndex,
                             std::size_t maximumRecords) {
    const std::vector<std::string> categories = categoryNames(data);
    // A 2D vector stores total copies, available copies and borrowing count
    // for every category. This demonstrates meaningful two-dimensional data.
    std::vector<std::vector<int>> statistics(
        categories.size(), std::vector<int>(3, 0));
    for (const Book& book : data.books) {
        if (!book.active) {
            continue;
        }
        for (std::size_t row = 0; row < categories.size(); ++row) {
            if (lowerCopy(categories[row]) == lowerCopy(book.category)) {
                statistics[row][0] += book.totalCopies;
                statistics[row][1] += book.availableCopies;
                statistics[row][2] += book.borrowCount;
                break;
            }
        }
    }

    out << "\nCATEGORY STATISTICS (2D VECTOR SUMMARY)\n"
        << std::string(65, '=') << '\n'
        << std::left << std::setw(28) << "Category" << std::right
        << std::setw(12) << "Copies" << std::setw(12) << "Available"
        << std::setw(13) << "Borrowings" << '\n' << std::string(65, '-') << '\n';
    std::size_t displayedCategories = 0;
    std::size_t row = startingIndex;
    while (row < categories.size() && displayedCategories < maximumRecords) {
        out << std::left << std::setw(28) << shortened(categories[row], 27)
            << std::right << std::setw(12) << statistics[row][0]
            << std::setw(12) << statistics[row][1] << std::setw(13)
            << statistics[row][2] << '\n';
        ++row;
        ++displayedCategories;
    }
    if (categories.empty()) {
        out << "No records found.\n";
    }
    out << std::string(65, '-') << "\nShowing " << displayedCategories
        << " of " << categories.size() << " categories.\n";
}

bool exportSelectedReport(const LibraryData& data, int reportType,
                          const Date& reportDate,
                          const std::string& reportTitle,
                          const std::string& filter,
                          const std::string& fileName,
                          std::string& message) {
    if (reportType < 1 || reportType > 6) {
        message = "Invalid report selection.";
        return false;
    }

    std::ofstream reportFile(fileName);
    if (!reportFile) {
        message = "Unable to open report file: " + fileName + ".";
        return false;
    }

    reportFile << std::string(78, '=') << '\n'
               << "TAR UMT Library Management System\n"
               << reportTitle << '\n'
               << "Report Type: " << reportTypeText(reportType) << '\n'
               << "Generated Date and Time: " << generatedDateAndTime() << '\n'
               << std::string(78, '=') << '\n';

    // Each selection uses the existing report function with its complete data.
    if (reportType == 1) {
        printMemberReport(data, reportFile, 0,
                          countMemberReportRecords(data));
    } else if (reportType == 2) {
        printInventoryReport(data, reportFile, 0,
                             countInventoryReportRecords(data));
    } else if (reportType == 3) {
        printBorrowingReport(data, reportFile, filter, 0,
                             countBorrowingReportRecords(data, filter));
    } else if (reportType == 4) {
        printOverdueReport(data, reportDate, reportFile, 0,
                           countOverdueReportRecords(data, reportDate));
    } else if (reportType == 5) {
        printPaymentReport(data, reportFile, filter, 0,
                           countPaymentReportRecords(data, filter));
    } else {
        printCategoryStatistics(data, reportFile, 0,
                                countCategoryStatisticsRecords(data));
    }

    if (!reportFile) {
        message = "An error occurred while writing report file: " + fileName + ".";
        reportFile.close();
        return false;
    }
    reportFile.close();
    if (!reportFile) {
        message = "An error occurred while closing report file: " + fileName + ".";
        return false;
    }

    message = "Report generated successfully: " + fileName + ".";
    return true;
}

}  // namespace lms
