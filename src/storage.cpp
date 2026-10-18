// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Loads and saves the shared library records in text files.

#include "storage.h"

#include "book_management.h"
#include "date_functions.h"
#include "input_validation.h"
#include "string_functions.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

using namespace std;

namespace fs = std::filesystem;

namespace lms {
namespace {

const int DATA_FILE_COUNT = 5;
const std::string DATA_FILE_NAMES[DATA_FILE_COUNT] = {
    "Member.txt", "Book.txt", "BorrowingHistory.txt", "Reservation.txt",
    "Payment.txt"};

// Builds a filename such as data/Member.txt from the selected data folder.
std::string dataFileName(const LibraryData& data, const std::string& fileName) {
    if (data.dataDirectory.empty()) {
        return fileName;
    }

    const char lastCharacter = data.dataDirectory[data.dataDirectory.size() - 1];
    if (lastCharacter == '/' || lastCharacter == '\\') {
        return data.dataDirectory + fileName;
    }
    return data.dataDirectory + "/" + fileName;
}

std::string loanStatusText(LoanStatus status) {
    if (status == LoanStatus::Active) {
        return "ACTIVE";
    }
    return "RETURNED";
}

bool parseLoanStatus(const std::string& text, LoanStatus& status) {
    if (text == "ACTIVE") {
        status = LoanStatus::Active;
        return true;
    }
    if (text == "RETURNED") {
        status = LoanStatus::Returned;
        return true;
    }
    return false;
}

std::string reservationStatusText(ReservationStatus status) {
    switch (status) {
        case ReservationStatus::Waiting:
            return "WAITING";
        case ReservationStatus::Ready:
            return "READY";
        case ReservationStatus::Fulfilled:
            return "FULFILLED";
        case ReservationStatus::Cancelled:
            return "CANCELLED";
    }
    return "WAITING";
}

bool parseReservationStatus(const std::string& text,
                            ReservationStatus& status) {
    if (text == "WAITING") {
        status = ReservationStatus::Waiting;
        return true;
    } else if (text == "READY") {
        status = ReservationStatus::Ready;
        return true;
    } else if (text == "FULFILLED") {
        status = ReservationStatus::Fulfilled;
        return true;
    } else if (text == "CANCELLED") {
        status = ReservationStatus::Cancelled;
        return true;
    }
    return false;
}

void setInvalidRecordMessage(const std::string& fileName, int lineNumber,
                             std::string& message,
                             const std::string& reason = "malformed fields") {
    message = "Invalid record in " + fileName + " at line " +
              std::to_string(lineNumber) + ": " + reason + ".";
}

// Confirms that no non-whitespace text remains after the expected row fields.
bool rowHasNoExtraData(std::istringstream& row) {
    row >> std::ws;
    return row.eof();
}

// Uses filesystem metadata so an unreadable file still counts as existing.
bool dataFileExists(const LibraryData& data, const std::string& fileName,
                    bool& exists, std::string& message) {
    const fs::path path = dataFileName(data, fileName);
    std::error_code error;
    exists = fs::exists(path, error);
    if (error) {
        message = "Filesystem inspection failed for " + path.string() + ": " +
                  error.message() + ".";
        return false;
    }
    if (!exists) {
        return true;
    }

    const bool regularFile = fs::is_regular_file(path, error);
    if (error) {
        message = "Filesystem inspection failed for " + path.string() + ": " +
                  error.message() + ".";
        return false;
    }
    if (!regularFile) {
        message = "Required data path exists but is not a regular file: " +
                  path.string() + ".";
        return false;
    }
    return true;
}

// Confirms that a required file exists before its physical read begins.
bool requiredDataFileExists(const LibraryData& data,
                            const std::string& fileName,
                            std::string& message) {
    bool exists = false;
    if (!dataFileExists(data, fileName, exists, message)) {
        return false;
    }
    if (!exists) {
        message = "Required data file does not exist: " +
                  dataFileName(data, fileName) + ".";
        return false;
    }
    return true;
}

// Copies one file so its original contents can be restored after a failed save.
bool copyFile(const std::string& sourceName, const std::string& targetName) {
    std::ifstream source(sourceName, std::ios::binary);
    if (!source) {
        return false;
    }
    std::ofstream target(targetName, std::ios::binary);
    if (!target) {
        return false;
    }
    char character = '\0';
    while (source.get(character)) {
        target.put(character);
        if (!target) {
            return false;
        }
    }
    const bool readSucceeded = source.eof() && !source.bad();
    target.close();
    return readSucceeded && static_cast<bool>(target);
}

// Creates every required backup before any current data file is overwritten.
bool createDataBackups(const LibraryData& data,
                       bool originalFiles[DATA_FILE_COUNT],
                       std::string& message) {
    for (int index = 0; index < DATA_FILE_COUNT; ++index) {
        const std::string originalName =
            dataFileName(data, DATA_FILE_NAMES[index]);
        const std::string backupName = originalName + ".bak";
        bool exists = false;
        if (!dataFileExists(data, DATA_FILE_NAMES[index], exists, message)) {
            return false;
        }
        originalFiles[index] = exists;
        if (originalFiles[index]) {
            if (!copyFile(originalName, backupName)) {
                for (int cleanupIndex = 0; cleanupIndex < DATA_FILE_COUNT;
                     ++cleanupIndex) {
                    const std::string cleanupName =
                        dataFileName(data, DATA_FILE_NAMES[cleanupIndex]) +
                        ".bak";
                    std::remove(cleanupName.c_str());
                }
                message = "Unable to create backup file: " + backupName +
                          ". Saving was cancelled.";
                return false;
            }
        } else {
            std::remove(backupName.c_str());
        }
    }
    return true;
}

// Restores old files and removes any new files created by an incomplete save.
bool restoreDataBackups(const LibraryData& data,
                        const bool originalFiles[DATA_FILE_COUNT]) {
    bool restorationSucceeded = true;
    for (int index = 0; index < DATA_FILE_COUNT; ++index) {
        const std::string originalName =
            dataFileName(data, DATA_FILE_NAMES[index]);
        const std::string backupName = originalName + ".bak";
        if (originalFiles[index]) {
            if (std::remove(originalName.c_str()) != 0) {
                restorationSucceeded = false;
                continue;
            }
            if (std::rename(backupName.c_str(), originalName.c_str()) != 0) {
                restorationSucceeded = false;
            }
        } else {
            std::remove(originalName.c_str());
            std::remove(backupName.c_str());
        }
    }
    return restorationSucceeded;
}

// Removes backup files after all five new files have been saved successfully.
bool removeDataBackups(const LibraryData& data, std::string& failedName) {
    bool cleanupSucceeded = true;
    for (int index = 0; index < DATA_FILE_COUNT; ++index) {
        const std::string backupName =
            dataFileName(data, DATA_FILE_NAMES[index]) + ".bak";
        std::error_code error;
        const bool exists = fs::exists(backupName, error);
        if (error) {
            failedName = backupName;
            cleanupSucceeded = false;
        } else if (exists) {
            if (std::remove(backupName.c_str()) != 0) {
                failedName = backupName;
                cleanupSucceeded = false;
            }
        }
    }
    return cleanupSucceeded;
}

// Counts present files while stopping safely on metadata inspection errors.
bool countExistingDataFiles(const LibraryData& data, int& count,
                            std::string& message) {
    count = 0;
    for (int index = 0; index < DATA_FILE_COUNT; ++index) {
        bool exists = false;
        if (!dataFileExists(data, DATA_FILE_NAMES[index], exists, message)) {
            return false;
        }
        if (exists) {
            ++count;
        }
    }
    return true;
}

// Creates the fixed data directory without replacing an existing file path.
bool createDataDirectory(const std::string& directory, std::string& message) {
    if (directory.empty() || directory == ".") {
        return true;
    }

    const fs::path path = directory;
    std::error_code error;
    const bool exists = fs::exists(path, error);
    if (error) {
        message = "Filesystem inspection failed for " + path.string() + ": " +
                  error.message() + ".";
        return false;
    }
    if (exists) {
        const bool directoryPath = fs::is_directory(path, error);
        if (error) {
            message = "Filesystem inspection failed for " + path.string() +
                      ": " + error.message() + ".";
            return false;
        }
        if (!directoryPath) {
            message = "Data directory path exists but is not a directory: " +
                      path.string() + ".";
            return false;
        }
        return true;
    }

    if (!fs::create_directories(path, error) || error) {
        message = "Unable to create data directory " + path.string() + ": " +
                  error.message() + ".";
        return false;
    }
    return true;
}

// Builds a practical startup error that names every missing required file.
bool incompleteDataMessage(const LibraryData& data, std::string& message) {
    message = "Incomplete data set. Missing required file(s): ";
    bool firstName = true;
    for (int index = 0; index < DATA_FILE_COUNT; ++index) {
        bool exists = false;
        std::string inspectionMessage;
        if (!dataFileExists(data, DATA_FILE_NAMES[index], exists,
                            inspectionMessage)) {
            message = inspectionMessage;
            return false;
        }
        if (!exists) {
            if (!firstName) {
                message += ", ";
            }
            message += DATA_FILE_NAMES[index];
            firstName = false;
        }
    }
    message +=
        ". The library requires Member.txt, Book.txt, BorrowingHistory.txt, "
        "Reservation.txt and Payment.txt.";
    return true;
}

bool hasDemonstrationDataThatNeedsUpgrade(const LibraryData& data) {
    if (data.members.size() != 2 || !data.reservations.empty()) {
        return false;
    }
    if (data.members[0].id != "M001" || data.members[1].id != "M002") {
        return false;
    }

    const bool originalSixBookData =
        data.books.size() == 6 && data.loans.empty() && data.payments.empty() &&
        data.books[0].id == "B001" && data.books[5].id == "B006";
    const bool expandedFifteenBookData =
        data.books.size() == 15 && data.loans.size() == 21 &&
        data.payments.size() == 63 && data.books[0].id == "B001" &&
        data.books[14].id == "B015";

    return originalSixBookData || expandedFifteenBookData;
}

// Adds one completed loan and its three cash records to demonstration data.
void addReturnedSampleLoan(LibraryData& data, const std::string& memberId,
                           const std::string& bookId,
                           const Date& borrowDate) {
    Loan loan;
    loan.id = data.nextLoanId;
    ++data.nextLoanId;
    loan.memberId = memberId;
    loan.bookId = bookId;
    loan.borrowDate = borrowDate;
    loan.dueDate = addDays(borrowDate, LOAN_PERIOD_DAYS);
    loan.returnDate = addDays(borrowDate, 7);
    loan.status = LoanStatus::Returned;
    loan.borrowingFee = BORROWING_FEE;
    loan.depositPaid = REFUNDABLE_DEPOSIT;
    loan.depositRefunded = REFUNDABLE_DEPOSIT;
    loan.returnedInGoodCondition = true;
    data.loans.push_back(loan);

    for (std::size_t index = 0; index < data.books.size(); ++index) {
        if (data.books[index].id == bookId) {
            ++data.books[index].borrowCount;
            break;
        }
    }

    Payment borrowingFee;
    borrowingFee.id = data.nextPaymentId;
    ++data.nextPaymentId;
    borrowingFee.memberId = memberId;
    borrowingFee.type = "BORROWING_FEE";
    borrowingFee.amount = BORROWING_FEE;
    borrowingFee.date = borrowDate;
    borrowingFee.referenceId = loan.id;
    borrowingFee.note = "Non-refundable borrowing fee";
    data.payments.push_back(borrowingFee);

    Payment deposit;
    deposit.id = data.nextPaymentId;
    ++data.nextPaymentId;
    deposit.memberId = memberId;
    deposit.type = "DEPOSIT";
    deposit.amount = REFUNDABLE_DEPOSIT;
    deposit.date = borrowDate;
    deposit.referenceId = loan.id;
    deposit.note = "Refundable book deposit received";
    data.payments.push_back(deposit);

    Payment refund;
    refund.id = data.nextPaymentId;
    ++data.nextPaymentId;
    refund.memberId = memberId;
    refund.type = "DEPOSIT_REFUND";
    refund.amount = -REFUNDABLE_DEPOSIT;
    refund.date = loan.returnDate;
    refund.referenceId = loan.id;
    refund.note = "Deposit refunded in full";
    data.payments.push_back(refund);
}

// Creates a chosen number of completed loans for one demonstration book.
void addSampleBookHistory(LibraryData& data, const std::string& bookId,
                          int borrowingCount, int startingOffset) {
    for (int index = 0; index < borrowingCount; ++index) {
        std::string memberId;
        if ((index + startingOffset) % 2 == 0) {
            memberId = "M001";
        } else {
            memberId = "M002";
        }
        const Date borrowDate =
            addDays(Date{2025, 1, 1}, startingOffset + index * 9);
        addReturnedSampleLoan(data, memberId, bookId, borrowDate);
    }
}

const Member* findLoadedMember(const LibraryData& data,
                               const std::string& memberId) {
    for (const Member& member : data.members) {
        if (lowerCopy(member.id) == lowerCopy(memberId)) {
            return &member;
        }
    }
    return nullptr;
}

const Book* findLoadedBook(const LibraryData& data, const std::string& bookId) {
    for (const Book& book : data.books) {
        if (lowerCopy(book.id) == lowerCopy(bookId)) {
            return &book;
        }
    }
    return nullptr;
}

bool memberExists(const LibraryData& data, const std::string& memberId) {
    return findLoadedMember(data, memberId) != nullptr;
}

bool bookExists(const LibraryData& data, const std::string& bookId) {
    return findLoadedBook(data, bookId) != nullptr;
}

bool activeReservationStatus(ReservationStatus status) {
    return status == ReservationStatus::Waiting ||
           status == ReservationStatus::Ready;
}

bool loanExists(const LibraryData& data, int loanId) {
    for (const Loan& loan : data.loans) {
        if (loan.id == loanId) {
            return true;
        }
    }
    return false;
}

bool reservationExists(const LibraryData& data, int reservationId) {
    for (const Reservation& reservation : data.reservations) {
        if (reservation.id == reservationId) {
            return true;
        }
    }
    return false;
}

bool knownPaymentType(const std::string& type) {
    return type == "BORROWING_FEE" || type == "DEPOSIT" ||
           type == "DEPOSIT_REFUND" || type == "RESERVATION_FEE" ||
           type == "FINE_CHARGE" || type == "FINE_PAYMENT";
}

// NOT_RECORDED keeps old Payment.txt rows compatible after methods were added.
bool knownPaymentMethod(const std::string& method) {
    return method == "CASH" || method == "E_WALLET" ||
           method == "ONLINE_BANKING" || method == "CARD" ||
           method == "NOT_RECORDED" || method == "NOT_APPLICABLE";
}

void setLoadedDataError(const LibraryData& data, const std::string& fileName,
                        const std::string& recordId,
                        const std::string& reason, std::string& message) {
    message = "Invalid loaded data in " + dataFileName(data, fileName) +
              " for record " + recordId + ": " + reason + ".";
}

// Validates IDs, relationships and stored values across all five loaded files.
bool validateLoadedData(const LibraryData& data, std::string& message) {
    for (std::size_t left = 0; left < data.members.size(); ++left) {
        const Member& member = data.members[left];
        const std::string recordId = member.id.empty() ? "<empty>" : member.id;
        if (!isValidIdentifier(member.id)) {
            setLoadedDataError(data, "Member.txt", recordId,
                               "member ID is empty or invalid", message);
            return false;
        }
        if (!isValidName(member.name)) {
            setLoadedDataError(data, "Member.txt", recordId,
                               "member name is invalid", message);
            return false;
        }
        if (!isValidPhone(member.phone)) {
            setLoadedDataError(data, "Member.txt", recordId,
                               "phone number is invalid", message);
            return false;
        }
        if (!isValidEmail(member.email)) {
            setLoadedDataError(data, "Member.txt", recordId,
                               "email address is invalid", message);
            return false;
        }
        if (!isValidPassword(member.password)) {
            setLoadedDataError(
                data, "Member.txt", recordId,
                "password must contain 6 to 30 characters with no whitespace",
                message);
            return false;
        }
        if (!std::isfinite(member.outstandingFine) ||
            member.outstandingFine < 0.0) {
            setLoadedDataError(data, "Member.txt", recordId,
                               "outstanding fine must be finite and non-negative",
                               message);
            return false;
        }
        for (std::size_t right = left + 1; right < data.members.size(); ++right) {
            if (lowerCopy(member.id) == lowerCopy(data.members[right].id)) {
                setLoadedDataError(data, "Member.txt", member.id,
                                   "duplicate member ID", message);
                return false;
            }
        }
    }

    for (std::size_t left = 0; left < data.books.size(); ++left) {
        const Book& book = data.books[left];
        const std::string recordId = book.id.empty() ? "<empty>" : book.id;
        if (!isValidIdentifier(book.id)) {
            setLoadedDataError(data, "Book.txt", recordId,
                               "book ID is empty or invalid", message);
            return false;
        }
        if (trim(book.title).empty() || book.title.size() > 100) {
            setLoadedDataError(data, "Book.txt", recordId,
                               "title must contain 1 to 100 characters",
                               message);
            return false;
        }
        if (trim(book.author).empty() || book.author.size() > 60) {
            setLoadedDataError(data, "Book.txt", recordId,
                               "author must contain 1 to 60 characters",
                               message);
            return false;
        }
        if (trim(book.category).empty() || book.category.size() > 40) {
            setLoadedDataError(data, "Book.txt", recordId,
                               "category must contain 1 to 40 characters",
                               message);
            return false;
        }
        if (!isValidBookCategory(book.category)) {
            setLoadedDataError(data, "Book.txt", recordId,
                               "category is not in the predefined category list",
                               message);
            return false;
        }
        if (book.totalCopies < 0 || book.availableCopies < 0 ||
            book.borrowCount < 0) {
            setLoadedDataError(data, "Book.txt", recordId,
                               "copy and borrowing values cannot be negative",
                               message);
            return false;
        }
        if (book.availableCopies > book.totalCopies) {
            setLoadedDataError(data, "Book.txt", book.id,
                               "available copies exceed total copies", message);
            return false;
        }
        for (std::size_t right = left + 1; right < data.books.size(); ++right) {
            if (lowerCopy(book.id) == lowerCopy(data.books[right].id)) {
                setLoadedDataError(data, "Book.txt", book.id,
                                   "duplicate book ID", message);
                return false;
            }
        }

        int loanCount = 0;
        int activeLoanCount = 0;
        for (const Loan& loan : data.loans) {
            if (lowerCopy(loan.bookId) == lowerCopy(book.id)) {
                ++loanCount;
                if (loan.status == LoanStatus::Active) {
                    ++activeLoanCount;
                }
            }
        }
        if (book.borrowCount != loanCount) {
            setLoadedDataError(data, "Book.txt", book.id,
                               "borrow count does not match loan records",
                               message);
            return false;
        }
        if (book.availableCopies != book.totalCopies - activeLoanCount) {
            setLoadedDataError(data, "Book.txt", book.id,
                               "available copies do not match active loans",
                               message);
            return false;
        }
    }

    for (std::size_t left = 0; left < data.loans.size(); ++left) {
        const Loan& loan = data.loans[left];
        const std::string recordId = std::to_string(loan.id);
        if (loan.id <= 0) {
            setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                               "loan ID must be positive", message);
            return false;
        }
        for (std::size_t right = left + 1; right < data.loans.size(); ++right) {
            if (loan.id == data.loans[right].id) {
                setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                                   "duplicate loan ID", message);
                return false;
            }
        }
        if (!memberExists(data, loan.memberId) ||
            !bookExists(data, loan.bookId)) {
            setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                               "referenced member or book does not exist",
                               message);
            return false;
        }
        if (!isValidDate(loan.borrowDate) || !isValidDate(loan.dueDate) ||
            daysBetween(loan.borrowDate, loan.dueDate) < 0) {
            setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                               "borrowing or due date is invalid", message);
            return false;
        }
        if (!std::isfinite(loan.borrowingFee) ||
            !std::isfinite(loan.depositPaid) ||
            !std::isfinite(loan.fineCharged) ||
            !std::isfinite(loan.depositRefunded) || loan.borrowingFee < 0.0 ||
            loan.depositPaid < 0.0 || loan.fineCharged < 0.0 ||
            loan.depositRefunded < 0.0) {
            setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                               "monetary values must be finite and non-negative",
                               message);
            return false;
        }
        if (loan.status == LoanStatus::Active &&
            isValidDate(loan.returnDate)) {
            setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                               "active loan must use '-' as its return date",
                               message);
            return false;
        }
        if (loan.status == LoanStatus::Returned &&
            (!isValidDate(loan.returnDate) ||
             daysBetween(loan.borrowDate, loan.returnDate) < 0)) {
            setLoadedDataError(data, "BorrowingHistory.txt", recordId,
                               "returned loan has an invalid return date",
                               message);
            return false;
        }
    }

    for (std::size_t left = 0; left < data.reservations.size(); ++left) {
        const Reservation& reservation = data.reservations[left];
        const std::string recordId = std::to_string(reservation.id);
        if (reservation.id <= 0) {
            setLoadedDataError(data, "Reservation.txt", recordId,
                               "reservation ID must be positive", message);
            return false;
        }
        for (std::size_t right = left + 1;
             right < data.reservations.size(); ++right) {
            if (reservation.id == data.reservations[right].id) {
                setLoadedDataError(data, "Reservation.txt", recordId,
                                   "duplicate reservation ID", message);
                return false;
            }
        }
        if (!memberExists(data, reservation.memberId) ||
            !bookExists(data, reservation.bookId)) {
            setLoadedDataError(data, "Reservation.txt", recordId,
                               "referenced member or book does not exist",
                               message);
            return false;
        }
        if (activeReservationStatus(reservation.status)) {
            const Member* member =
                findLoadedMember(data, reservation.memberId);
            const Book* book = findLoadedBook(data, reservation.bookId);
            if (member == nullptr || book == nullptr || !member->active ||
                !book->active) {
                setLoadedDataError(
                    data, "Reservation.txt", recordId,
                    "active reservation requires an active member and book",
                    message);
                return false;
            }
            for (std::size_t right = left + 1;
                 right < data.reservations.size(); ++right) {
                const Reservation& other = data.reservations[right];
                if (activeReservationStatus(other.status) &&
                    lowerCopy(reservation.memberId) ==
                        lowerCopy(other.memberId) &&
                    lowerCopy(reservation.bookId) == lowerCopy(other.bookId)) {
                    setLoadedDataError(
                        data, "Reservation.txt", recordId,
                        "duplicate active reservation for the same member and book",
                        message);
                    return false;
                }
            }
        }
        if (!isValidDate(reservation.reservationDate) ||
            !isValidDate(reservation.estimatedAvailableDate) ||
            daysBetween(reservation.reservationDate,
                        reservation.estimatedAvailableDate) < 0) {
            setLoadedDataError(data, "Reservation.txt", recordId,
                               "reservation dates are invalid", message);
            return false;
        }
        if (!std::isfinite(reservation.reservationFee) ||
            reservation.reservationFee < 0.0) {
            setLoadedDataError(data, "Reservation.txt", recordId,
                               "reservation fee must be finite and non-negative",
                               message);
            return false;
        }
    }

    for (const Book& book : data.books) {
        int readyReservationCount = 0;
        for (const Reservation& reservation : data.reservations) {
            if (reservation.status == ReservationStatus::Ready &&
                lowerCopy(reservation.bookId) == lowerCopy(book.id)) {
                ++readyReservationCount;
            }
        }
        if (readyReservationCount > book.availableCopies) {
            setLoadedDataError(
                data, "Reservation.txt", book.id,
                "READY reservations exceed the available copies for this book",
                message);
            return false;
        }
    }

    for (std::size_t left = 0; left < data.payments.size(); ++left) {
        const Payment& payment = data.payments[left];
        const std::string recordId = std::to_string(payment.id);
        if (payment.id <= 0) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "payment ID must be positive", message);
            return false;
        }
        for (std::size_t right = left + 1; right < data.payments.size(); ++right) {
            if (payment.id == data.payments[right].id) {
                setLoadedDataError(data, "Payment.txt", recordId,
                                   "duplicate payment ID", message);
                return false;
            }
        }
        if (!memberExists(data, payment.memberId)) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "referenced member does not exist", message);
            return false;
        }
        if (!knownPaymentType(payment.type)) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "unknown payment type " + payment.type, message);
            return false;
        }
        if (!knownPaymentMethod(payment.method)) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "unknown payment method " + payment.method,
                               message);
            return false;
        }
        if (!std::isfinite(payment.amount) ||
            (payment.type == "DEPOSIT_REFUND" && payment.amount >= 0.0) ||
            (payment.type != "DEPOSIT_REFUND" && payment.amount <= 0.0)) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "payment amount has an invalid sign or value",
                               message);
            return false;
        }
        if (!isValidDate(payment.date)) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "payment date is invalid", message);
            return false;
        }
        if (payment.type == "RESERVATION_FEE") {
            if (!reservationExists(data, payment.referenceId)) {
                setLoadedDataError(data, "Payment.txt", recordId,
                                   "referenced reservation does not exist",
                                   message);
                return false;
            }
        } else if (payment.type == "FINE_PAYMENT") {
            if (payment.referenceId != 0) {
                setLoadedDataError(data, "Payment.txt", recordId,
                                   "fine payment reference ID must be 0",
                                   message);
                return false;
            }
        } else if (!loanExists(data, payment.referenceId)) {
            setLoadedDataError(data, "Payment.txt", recordId,
                               "referenced loan does not exist", message);
            return false;
        }
    }

    // Stored balances must agree with assessed fines minus received payments.
    for (const Member& member : data.members) {
        double calculatedBalance = 0.0;
        for (const Payment& payment : data.payments) {
            if (lowerCopy(payment.memberId) == lowerCopy(member.id)) {
                if (payment.type == "FINE_CHARGE") {
                    calculatedBalance += payment.amount;
                } else if (payment.type == "FINE_PAYMENT") {
                    calculatedBalance -= payment.amount;
                }
            }
        }
        if (std::fabs(member.outstandingFine - calculatedBalance) > 0.01) {
            std::ostringstream calculatedText;
            calculatedText << std::fixed << std::setprecision(2)
                           << calculatedBalance;
            setLoadedDataError(
                data, "Member.txt", member.id,
                "outstanding fine does not match the calculated balance of RM" +
                    calculatedText.str(),
                message);
            return false;
        }
    }
    return true;
}

}  // namespace

// Loads member records from Member.txt into the members vector.
bool loadMembers(LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Member.txt");
    data.members.clear();
    if (!requiredDataFileExists(data, "Member.txt", message)) {
        return false;
    }
    std::ifstream input(fileName);
    if (!input) {
        message = "Data file exists but cannot be opened for reading: " +
                  fileName + ".";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) {
            continue;
        }

        // Quoted fields preserve names and other text that contain spaces.
        std::istringstream row(line);
        Member member;
        int active = 0;
        if (!(row >> std::quoted(member.id) >> std::quoted(member.password) >>
              std::quoted(member.name) >> std::quoted(member.phone) >>
              std::quoted(member.email) >> active >> member.outstandingFine)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "member fields could not be parsed");
            return false;
        }
        if (!rowHasNoExtraData(row)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "extra trailing data was found");
            return false;
        }
        if (active != 0 && active != 1) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "active value must be 0 or 1");
            return false;
        }
        member.active = active != 0;
        data.members.push_back(member);
    }
    if (input.bad()) {
        data.members.clear();
        message = "Read error occurred while loading " + fileName +
                  ". No data was changed.";
        return false;
    }
    return true;
}

// Loads catalogue and inventory records from Book.txt.
bool loadBooks(LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Book.txt");
    data.books.clear();
    if (!requiredDataFileExists(data, "Book.txt", message)) {
        return false;
    }
    std::ifstream input(fileName);
    if (!input) {
        message = "Data file exists but cannot be opened for reading: " +
                  fileName + ".";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) {
            continue;
        }

        std::istringstream row(line);
        Book book;
        int active = 0;
        if (!(row >> std::quoted(book.id) >> std::quoted(book.title) >>
              std::quoted(book.author) >> std::quoted(book.category) >>
              book.totalCopies >> book.availableCopies >> book.borrowCount >>
              active)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "book fields could not be parsed");
            return false;
        }
        if (!rowHasNoExtraData(row)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "extra trailing data was found");
            return false;
        }
        if (active != 0 && active != 1) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "active value must be 0 or 1");
            return false;
        }
        book.active = active != 0;
        data.books.push_back(book);
    }
    if (input.bad()) {
        data.books.clear();
        message = "Read error occurred while loading " + fileName +
                  ". No data was changed.";
        return false;
    }
    return true;
}

// Loads borrowing and return history from BorrowingHistory.txt.
bool loadLoans(LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "BorrowingHistory.txt");
    data.loans.clear();
    if (!requiredDataFileExists(data, "BorrowingHistory.txt", message)) {
        return false;
    }
    std::ifstream input(fileName);
    if (!input) {
        message = "Data file exists but cannot be opened for reading: " +
                  fileName + ".";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) {
            continue;
        }

        std::istringstream row(line);
        Loan loan;
        std::string borrowDate;
        std::string dueDate;
        std::string returnDate;
        std::string status;
        int goodCondition = 1;
        if (!(row >> loan.id >> std::quoted(loan.memberId) >>
              std::quoted(loan.bookId) >> std::quoted(borrowDate) >>
              std::quoted(dueDate) >> std::quoted(returnDate) >>
              std::quoted(status) >> loan.borrowingFee >> loan.depositPaid >>
              loan.fineCharged >> loan.depositRefunded >> goodCondition)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "loan fields could not be parsed");
            return false;
        }
        if (!rowHasNoExtraData(row)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "extra trailing data was found");
            return false;
        }
        if (goodCondition != 0 && goodCondition != 1) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "condition value must be 0 or 1");
            return false;
        }
        if (!parseDate(borrowDate, loan.borrowDate) ||
            !parseDate(dueDate, loan.dueDate)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "borrowing or due date is invalid");
            return false;
        }
        // A dash represents an active loan that has no return date yet.
        if (returnDate != "-" && !parseDate(returnDate, loan.returnDate)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "return date is invalid");
            return false;
        }
        if (!parseLoanStatus(status, loan.status)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "unknown loan status " + status);
            return false;
        }
        loan.returnedInGoodCondition = goodCondition != 0;
        data.loans.push_back(loan);
    }
    if (input.bad()) {
        data.loans.clear();
        message = "Read error occurred while loading " + fileName +
                  ". No data was changed.";
        return false;
    }
    return true;
}

// Loads FIFO reservation records from Reservation.txt.
bool loadReservations(LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Reservation.txt");
    data.reservations.clear();
    if (!requiredDataFileExists(data, "Reservation.txt", message)) {
        return false;
    }
    std::ifstream input(fileName);
    if (!input) {
        message = "Data file exists but cannot be opened for reading: " +
                  fileName + ".";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) {
            continue;
        }

        std::istringstream row(line);
        Reservation reservation;
        std::string reservationDate;
        std::string estimatedDate;
        std::string status;
        int notified = 0;
        if (!(row >> reservation.id >> std::quoted(reservation.memberId) >>
              std::quoted(reservation.bookId) >>
              std::quoted(reservationDate) >> std::quoted(estimatedDate) >>
              std::quoted(status) >> reservation.reservationFee >> notified)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "reservation fields could not be parsed");
            return false;
        }
        if (!rowHasNoExtraData(row)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "extra trailing data was found");
            return false;
        }
        if (notified != 0 && notified != 1) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "notified value must be 0 or 1");
            return false;
        }
        if (!parseDate(reservationDate, reservation.reservationDate) ||
            !parseDate(estimatedDate, reservation.estimatedAvailableDate)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "reservation date is invalid");
            return false;
        }
        if (!parseReservationStatus(status, reservation.status)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "unknown reservation status " + status);
            return false;
        }
        reservation.notified = notified != 0;
        data.reservations.push_back(reservation);
    }
    if (input.bad()) {
        data.reservations.clear();
        message = "Read error occurred while loading " + fileName +
                  ". No data was changed.";
        return false;
    }
    return true;
}

// Loads fees, deposits, refunds and fine transactions from Payment.txt.
bool loadPayments(LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Payment.txt");
    data.payments.clear();
    if (!requiredDataFileExists(data, "Payment.txt", message)) {
        return false;
    }
    std::ifstream input(fileName);
    if (!input) {
        message = "Data file exists but cannot be opened for reading: " +
                  fileName + ".";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (trim(line).empty()) {
            continue;
        }

        std::istringstream row(line);
        Payment payment;
        std::string paymentDate;
        if (!(row >> payment.id >> std::quoted(payment.memberId) >>
              std::quoted(payment.type))) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "payment fields could not be parsed");
            return false;
        }
        // New rows contain a quoted payment method. Old rows have the amount
        // immediately after the type and are kept as NOT_RECORDED.
        row >> std::ws;
        if (row.peek() == '"') {
            if (!(row >> std::quoted(payment.method))) {
                setInvalidRecordMessage(fileName, lineNumber, message,
                                        "payment method could not be parsed");
                return false;
            }
        } else {
            payment.method = "NOT_RECORDED";
        }
        if (!(row >> payment.amount >> std::quoted(paymentDate) >>
              payment.referenceId >> std::quoted(payment.note))) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "payment fields could not be parsed");
            return false;
        }
        if (!rowHasNoExtraData(row)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "extra trailing data was found");
            return false;
        }
        if (!parseDate(paymentDate, payment.date)) {
            setInvalidRecordMessage(fileName, lineNumber, message,
                                    "payment date is invalid");
            return false;
        }
        data.payments.push_back(payment);
    }
    if (input.bad()) {
        data.payments.clear();
        message = "Read error occurred while loading " + fileName +
                  ". No data was changed.";
        return false;
    }
    return true;
}

// Saves every member record using the existing quoted text-file format.
bool saveMembers(const LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Member.txt");
    std::ofstream output(fileName);
    if (!output) {
        message = "Unable to write data file: " + fileName;
        return false;
    }

    // Fixed precision keeps money values consistent when they are loaded again.
    output << std::fixed << std::setprecision(2);
    for (const Member& member : data.members) {
        output << std::quoted(member.id) << ' ' << std::quoted(member.password)
               << ' ' << std::quoted(member.name) << ' '
               << std::quoted(member.phone) << ' ' << std::quoted(member.email)
               << ' ' << member.active << ' ' << member.outstandingFine << '\n';
    }
    const bool writeSucceeded = static_cast<bool>(output);
    output.close();
    if (!writeSucceeded || !output) {
        message = "A write or close error occurred for data file: " + fileName;
        return false;
    }
    return true;
}

// Saves all catalogue and inventory records to Book.txt.
bool saveBooks(const LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Book.txt");
    std::ofstream output(fileName);
    if (!output) {
        message = "Unable to write data file: " + fileName;
        return false;
    }

    for (const Book& book : data.books) {
        output << std::quoted(book.id) << ' ' << std::quoted(book.title) << ' '
               << std::quoted(book.author) << ' ' << std::quoted(book.category)
               << ' ' << book.totalCopies << ' ' << book.availableCopies << ' '
               << book.borrowCount << ' ' << book.active << '\n';
    }
    const bool writeSucceeded = static_cast<bool>(output);
    output.close();
    if (!writeSucceeded || !output) {
        message = "A write or close error occurred for data file: " + fileName;
        return false;
    }
    return true;
}

// Saves borrowing and return records to BorrowingHistory.txt.
bool saveLoans(const LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "BorrowingHistory.txt");
    std::ofstream output(fileName);
    if (!output) {
        message = "Unable to write data file: " + fileName;
        return false;
    }

    output << std::fixed << std::setprecision(2);
    for (const Loan& loan : data.loans) {
        output << loan.id << ' ' << std::quoted(loan.memberId) << ' '
               << std::quoted(loan.bookId) << ' '
               << std::quoted(formatDate(loan.borrowDate)) << ' '
               << std::quoted(formatDate(loan.dueDate)) << ' '
               << std::quoted(formatDate(loan.returnDate)) << ' '
               << std::quoted(loanStatusText(loan.status)) << ' '
               << loan.borrowingFee << ' ' << loan.depositPaid << ' '
               << loan.fineCharged << ' ' << loan.depositRefunded << ' '
               << loan.returnedInGoodCondition << '\n';
    }
    const bool writeSucceeded = static_cast<bool>(output);
    output.close();
    if (!writeSucceeded || !output) {
        message = "A write or close error occurred for data file: " + fileName;
        return false;
    }
    return true;
}

// Saves the complete FIFO reservation history to Reservation.txt.
bool saveReservations(const LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Reservation.txt");
    std::ofstream output(fileName);
    if (!output) {
        message = "Unable to write data file: " + fileName;
        return false;
    }

    output << std::fixed << std::setprecision(2);
    for (const Reservation& reservation : data.reservations) {
        output << reservation.id << ' ' << std::quoted(reservation.memberId)
               << ' ' << std::quoted(reservation.bookId) << ' '
               << std::quoted(formatDate(reservation.reservationDate)) << ' '
               << std::quoted(formatDate(reservation.estimatedAvailableDate))
               << ' ' << std::quoted(reservationStatusText(reservation.status))
               << ' ' << reservation.reservationFee << ' '
               << reservation.notified << '\n';
    }
    const bool writeSucceeded = static_cast<bool>(output);
    output.close();
    if (!writeSucceeded || !output) {
        message = "A write or close error occurred for data file: " + fileName;
        return false;
    }
    return true;
}

// Saves all payment and accounting records to Payment.txt.
bool savePayments(const LibraryData& data, std::string& message) {
    const std::string fileName = dataFileName(data, "Payment.txt");
    std::ofstream output(fileName);
    if (!output) {
        message = "Unable to write data file: " + fileName;
        return false;
    }

    output << std::fixed << std::setprecision(2);
    for (const Payment& payment : data.payments) {
        output << payment.id << ' ' << std::quoted(payment.memberId) << ' '
               << std::quoted(payment.type) << ' '
               << std::quoted(payment.method) << ' ' << payment.amount << ' '
               << std::quoted(formatDate(payment.date)) << ' '
               << payment.referenceId << ' ' << std::quoted(payment.note)
               << '\n';
    }
    const bool writeSucceeded = static_cast<bool>(output);
    output.close();
    if (!writeSucceeded || !output) {
        message = "A write or close error occurred for data file: " + fileName;
        return false;
    }
    return true;
}

// Loads all five data files and calculates the next available record IDs.
bool loadAllData(LibraryData& data, std::string& message) {
    data.nextLoanId = 1;
    data.nextReservationId = 1;
    data.nextPaymentId = 1;

    // Each file fills one vector inside the same shared LibraryData object.
    if (!loadMembers(data, message) || !loadBooks(data, message) ||
        !loadLoans(data, message) || !loadReservations(data, message) ||
        !loadPayments(data, message)) {
        return false;
    }
    if (!validateLoadedData(data, message)) {
        return false;
    }

    // Continue IDs after the largest stored record to prevent duplicates.
    for (const Loan& loan : data.loans) {
        if (loan.id >= data.nextLoanId) {
            data.nextLoanId = loan.id + 1;
        }
    }
    for (const Reservation& reservation : data.reservations) {
        if (reservation.id >= data.nextReservationId) {
            data.nextReservationId = reservation.id + 1;
        }
    }
    for (const Payment& payment : data.payments) {
        if (payment.id >= data.nextPaymentId) {
            data.nextPaymentId = payment.id + 1;
        }
    }
    return true;
}

// Saves each vector while backups protect the previous complete file set.
bool saveAllData(const LibraryData& data, std::string& message) {
    bool originalFiles[DATA_FILE_COUNT] = {false, false, false, false, false};
    // Back up every existing file before beginning the five-file save.
    if (!createDataBackups(data, originalFiles, message)) {
        return false;
    }

    // Save each vector through its matching text-file function.
    const bool saved = saveMembers(data, message) && saveBooks(data, message) &&
                       saveLoans(data, message) &&
                       saveReservations(data, message) &&
                       savePayments(data, message);
    if (!saved) {
        const std::string saveError = message;
        // Restore the full old set if any one of the five saves fails.
        if (!restoreDataBackups(data, originalFiles)) {
            message = saveError +
                      " Original files could not be fully restored; manual "
                      "recovery may be required.";
        } else {
            message = saveError + " Original files were restored.";
        }
        return false;
    }

    // Clean up backups only after every current file has been written.
    std::string failedBackupName;
    if (!removeDataBackups(data, failedBackupName)) {
        message = "Data saved successfully, but backup file " +
                  failedBackupName + " could not be removed.";
        return true;
    }

    message = "Data saved successfully.";
    return true;
}

// Loads existing files or creates the original demonstration dataset.
bool initializeLibrary(LibraryData& data, const std::string& dataDirectory,
                       std::string& message) {
    // Start with empty shared data, then validate the complete five-file set.
    data = LibraryData{};
    data.dataDirectory = dataDirectory;

    int existingFileCount = 0;
    if (!countExistingDataFiles(data, existingFileCount, message)) {
        return false;
    }
    if (existingFileCount > 0 && existingFileCount < DATA_FILE_COUNT) {
        if (!incompleteDataMessage(data, message)) {
            return false;
        }
        return false;
    }

    if (existingFileCount == 0) {
        if (!createDataDirectory(dataDirectory, message)) {
            return false;
        }
        seedSampleData(data);
        if (!saveAllData(data, message)) {
            return false;
        }
        message = "New data files created with demonstration records.";
        return true;
    }

    if (!loadAllData(data, message)) {
        return false;
    }

    if (hasDemonstrationDataThatNeedsUpgrade(data)) {
        seedSampleData(data);
        if (!saveAllData(data, message)) {
            return false;
        }
        message = "Demonstration catalogue and borrowing history upgraded.";
    } else {
        message = "Existing library data loaded successfully.";
    }
    return true;
}

// Creates 150 books and borrowing histories used for classroom demonstration.
void seedSampleData(LibraryData& data) {
    data.nextLoanId = 1;
    data.nextReservationId = 1;
    data.nextPaymentId = 1;
    data.loans.clear();
    data.reservations.clear();
    data.payments.clear();

    data.members = {
        Member{"M001", "pass123", "Alicia Tan", "+60123456789",
               "alicia@example.com", true, 0.0},
        Member{"M002", "pass123", "Brandon Lee", "+60139876543",
               "brandon@example.com", true, 0.0}};

    data.books = {
        Book{"B001", "The C++ Programming Language", "Bjarne Stroustrup",
             "Technology", 2, 2, 0, true},
        Book{"B002", "Clean Code", "Robert C. Martin", "Technology", 2, 2,
             0, true},
        Book{"B003", "The Hobbit", "J.R.R. Tolkien", "Fantasy", 3, 3, 0,
             true},
        Book{"B004", "Atomic Habits", "James Clear", "Self Development", 2,
             2, 0, true},
        Book{"B005", "A Brief History of Time", "Stephen Hawking", "Science",
             1, 1, 0, true},
        Book{"B006", "Pride and Prejudice", "Jane Austen", "Classic", 2, 2,
             0, true},
        Book{"B007", "Nineteen Eighty-Four", "George Orwell", "Dystopian", 2,
             2, 0, true},
        Book{"B008", "Harry Potter and the Philosopher's Stone", "J.K. Rowling",
             "Fantasy", 3, 3, 0, true},
        Book{"B009", "To Kill a Mockingbird", "Harper Lee", "Classic", 2, 2,
             0, true},
        Book{"B010", "The Alchemist", "Paulo Coelho", "Fiction", 2, 2, 0,
             true},
        Book{"B011", "Introduction to Algorithms", "Thomas H. Cormen",
             "Technology", 2, 2, 0, true},
        Book{"B012", "Sapiens", "Yuval Noah Harari", "History", 2, 2, 0,
             true},
        Book{"B013", "The Psychology of Money", "Morgan Housel", "Finance", 2,
             2, 0, true},
        Book{"B014", "The Great Gatsby", "F. Scott Fitzgerald", "Classic", 2,
             2, 0, true},
        Book{"B015", "The Little Prince", "Antoine de Saint-Exupery",
             "Fiction", 2, 2, 0, true}};

    const std::vector<std::string>& categories = bookCategories();
    for (int number = 16; number <= 150; ++number) {
        std::ostringstream id;
        id << 'B' << std::setfill('0') << std::setw(3) << number;

        std::ostringstream title;
        title << "Library Collection Volume " << std::setfill('0')
              << std::setw(3) << number;

        std::ostringstream author;
        author << "Collection Author " << std::setfill('0')
               << std::setw(3) << number;

        const int copies = 1 + number % 3;
        Book book;
        book.id = id.str();
        book.title = title.str();
        book.author = author.str();
        book.category = categories[static_cast<std::size_t>(number - 16) %
                                   categories.size()];
        book.totalCopies = copies;
        book.availableCopies = copies;
        book.active = true;
        data.books.push_back(book);
    }

    addSampleBookHistory(data, "B003", 30, 0);
    addSampleBookHistory(data, "B004", 27, 1);
    addSampleBookHistory(data, "B012", 24, 2);
    addSampleBookHistory(data, "B002", 20, 3);
    addSampleBookHistory(data, "B001", 16, 4);
}

}  // namespace lms
