// Shared User Interface and Module Integration
// Contributors: All four group members
// Purpose: Displays menus and connects user actions to the four modules.
// Interface responsibilities:
// - Lam Yi Hen: member registration, profile and account screens
// - Tan Yi Heng: book search, borrowing, returns and inventory screens
// - Cheah Yong Shun: fine payment and payment-monitoring screens
// - Lim Jun Yi: report, statistics and report-export screens

#include "ui.h"

#include "additional_functionalities.h"
#include "book_management.h"
#include "book_borrowing.h"
#include "date_functions.h"
#include "input_validation.h"
#include "member_management.h"
#include "payment_functions.h"
#include "record_search.h"
#include "reporting.h"
#include "storage.h"
#include "string_functions.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace std;

namespace fs = std::filesystem;

namespace lms {
namespace {

void waitForEnter(
    const std::string& message = "Press Enter to return to the previous menu...") {
    std::cout << "\n" << message;
    std::string ignored;
    readConsoleLineOrExit(ignored);
}

void printHeading(const std::string& title) {
    std::cout << "\n" << std::string(78, '=') << "\n  " << title << "\n"
              << std::string(78, '=') << "\n";
}

std::string shortText(const std::string& text, std::size_t width) {
    return text.size() <= width ? text : text.substr(0, width - 3) + "...";
}

// Validates catalogue text immediately instead of waiting until every field
// has been entered. The same rules are checked again by book_management.cpp.
std::string readBookText(const std::string& prompt, std::size_t maximumLength,
                         const std::string& fieldName) {
    while (true) {
        const std::string value = readRequiredLine(prompt);
        if (value.size() <= maximumLength && value.find('|') == std::string::npos) {
            return value;
        }
        std::cout << "Invalid " << fieldName << ". Enter up to "
                  << maximumLength << " characters without '|'.\n";
    }
}

// Presents the shared book categories so category spelling stays consistent.
std::string readBookCategory() {
    const std::vector<std::string>& categories = bookCategories();
    std::cout << "\nSelect Book Category:\n";
    for (std::size_t index = 0; index < categories.size(); ++index) {
        std::cout << index + 1 << ". " << categories[index] << '\n';
    }
    const int choice = readIntInRange(
        "Enter your choice: ", 1, static_cast<int>(categories.size()));
    return categories[static_cast<std::size_t>(choice - 1)];
}

// Holds the simple checkout details collected before a member payment is saved.
struct CheckoutDetails {
    std::string method;
    double cashReceived = 0.0;
    double change = 0.0;
};

// Returns true only when every character is a numeric digit.
bool containsOnlyDigits(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(value[index]);
        if (std::isdigit(character) == 0) {
            return false;
        }
    }
    return true;
}

// Removes spaces from a card number before it is checked. The value is not saved.
std::string cardNumberWithoutSpaces(const std::string& enteredNumber) {
    std::string number;
    for (std::size_t index = 0; index < enteredNumber.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(enteredNumber[index]);
        if (std::isspace(character) == 0) {
            number += enteredNumber[index];
        }
    }
    return number;
}

// Reads a card number with 16 digits. Spaces entered by the user are ignored.
void readValidCardNumber() {
    while (true) {
        const std::string enteredNumber = readRequiredLine("Card Number: ");
        const std::string number = cardNumberWithoutSpaces(enteredNumber);
        if (number.size() == 16 && containsOnlyDigits(number)) {
            return;
        }
        std::cout << "Invalid card number. Enter exactly 16 digits; spaces are allowed.\n";
    }
}

// Reads MM/YY and rejects cards that expired before the current month.
void readValidCardExpiryDate() {
    while (true) {
        const std::string expiryDate = readRequiredLine("Expiry Date (MM/YY): ");
        const bool correctFormat =
            expiryDate.size() == 5 && expiryDate[2] == '/' &&
            std::isdigit(static_cast<unsigned char>(expiryDate[0])) != 0 &&
            std::isdigit(static_cast<unsigned char>(expiryDate[1])) != 0 &&
            std::isdigit(static_cast<unsigned char>(expiryDate[3])) != 0 &&
            std::isdigit(static_cast<unsigned char>(expiryDate[4])) != 0;
        if (correctFormat) {
            const int month = (expiryDate[0] - '0') * 10 + expiryDate[1] - '0';
            const int year = 2000 + (expiryDate[3] - '0') * 10 + expiryDate[4] - '0';
            const Date today = todayLocal();
            if (month >= 1 && month <= 12 &&
                (year > today.year ||
                 (year == today.year && month >= today.month))) {
                return;
            }
        }
        std::cout << "Invalid or expired expiry date. Use MM/YY and a current "
                     "or future month.\n";
    }
}

// Reads a three-digit CVV without storing the sensitive value.
void readValidCardCvv() {
    while (true) {
        const std::string cvv = readRequiredLine("CVV: ");
        if (cvv.size() == 3 && containsOnlyDigits(cvv)) {
            return;
        }
        std::cout << "Invalid CVV. Enter exactly 3 digits.\n";
    }
}

// Gives the user a clear Enter-to-confirm action for digital transfers.
void confirmDuitNowTransfer() {
    std::cout << "Thank you for transferring the amount to DuitNow ID: "
              << "202301045678.\n"
              << "Press Enter to confirm payment...";
    std::string confirmation;
    readConsoleLineOrExit(confirmation);
}

// Collects the required card fields without saving sensitive card information.
void readCardPaymentDetails() {
    std::cout << "\nCard payment details\n";
    readValidCardNumber();
    readValidCardExpiryDate();
    readValidCardCvv();
    std::cout << "All card details are valid. Press Enter to confirm payment...";
    std::string confirmation;
    readConsoleLineOrExit(confirmation);
}

// Prints a clear confirmation only after the related payment has been saved.
void printPaymentSuccess(const CheckoutDetails& payment) {
    std::cout << "Payment status : Successful\n"
              << "Payment method : " << paymentMethodLabel(payment.method) << '\n';
}

// Lets a member choose a payment method for fees, deposits or fine payments.
CheckoutDetails readCheckoutDetails(double amountDue) {
    CheckoutDetails details;
    std::cout << "\nAmount due: RM" << std::fixed << std::setprecision(2)
              << amountDue << "\n\n"
              << "Payment method\n"
              << "1. Cash\n"
              << "2. E-wallet\n"
              << "3. Online Banking\n"
              << "4. Card\n";
    const int choice = readIntInRange("Select payment method: ", 1, 4);
    if (choice == 1) {
        details.method = "CASH";
        while (true) {
            details.cashReceived =
                readPositiveAmount("Cash received: RM", 999999.99);
            if (details.cashReceived + 0.005 >= amountDue) {
                details.change = details.cashReceived - amountDue;
                std::cout << "Change to return: RM" << std::fixed
                          << std::setprecision(2) << details.change << '\n';
                break;
            }
            std::cout << "Insufficient cash. Enter at least RM" << std::fixed
                      << std::setprecision(2) << amountDue << ".\n";
        }
    } else if (choice == 2) {
        details.method = "E_WALLET";
        details.cashReceived = amountDue;
    } else if (choice == 3) {
        details.method = "ONLINE_BANKING";
        details.cashReceived = amountDue;
    } else {
        details.method = "CARD";
        details.cashReceived = amountDue;
    }
    std::cout << "Selected payment method: "
              << paymentMethodLabel(details.method) << "\n";
    if (details.method == "E_WALLET" || details.method == "ONLINE_BANKING") {
        confirmDuitNowTransfer();
    } else if (details.method == "CARD") {
        readCardPaymentDetails();
    }
    return details;
}

constexpr std::size_t RECORDS_PER_PAGE = 20;

std::size_t numberOfPages(std::size_t recordCount, std::size_t pageSize) {
    if (recordCount == 0 || pageSize == 0) {
        return 1;
    }
    return (recordCount + pageSize - 1) / pageSize;
}

void printRecordPageInformation(std::size_t currentPage,
                                std::size_t recordCount,
                                std::size_t pageSize) {
    std::cout << "\nPage " << currentPage + 1 << " of "
              << numberOfPages(recordCount, pageSize)
              << " | Records: " << recordCount << '\n';
}

// Reads one simple page command and changes the page only when it is valid.
bool selectAnotherPage(std::size_t& currentPage, std::size_t totalPages) {
    while (true) {
        std::cout << "\nN = Next page | P = Previous page | 0 = Return\n"
                  << "Command: ";
        std::string command;
        readConsoleLineOrExit(command);
        command = lowerCopy(trim(command));

        if (command == "n") {
            if (currentPage + 1 >= totalPages) {
                std::cout << "You are already on the last page.\n";
            } else {
                ++currentPage;
                return true;
            }
        } else if (command == "p") {
            if (currentPage == 0) {
                std::cout << "You are already on the first page.\n";
            } else {
                --currentPage;
                return true;
            }
        } else if (command == "0") {
            return false;
        } else {
            std::cout << "Invalid command. Enter N, P or 0.\n";
        }
    }
}

void sortLoansNewestFirst(std::vector<const Loan*>& loans) {
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
}

void sortPaymentsNewestFirst(std::vector<const Payment*>& payments) {
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
    return "UNKNOWN";
}

void printResult(const OperationResult& result) {
    std::cout << (result.success ? "\nSUCCESS: " : "\nERROR: ")
              << result.message << "\n";
}

void printBookPage(const LibraryData& data,
                   const std::vector<const Book*>& books,
                   std::size_t currentPage, std::size_t pageSize);

void printBookList(const LibraryData& data,
                   const std::vector<const Book*>& books) {
    if (books.empty()) {
        std::cout << "No matching books found.\n";
        return;
    }

    const std::size_t totalPages =
        numberOfPages(books.size(), RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printBookPage(data, books, currentPage, RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void printBookPage(const LibraryData& data,
                   const std::vector<const Book*>& books,
                   std::size_t currentPage, std::size_t pageSize) {
    if (books.empty() || pageSize == 0) {
        return;
    }

    const std::size_t totalPages = (books.size() + pageSize - 1) / pageSize;
    if (currentPage >= totalPages) {
        return;
    }

    // Calculate the safe range containing only the selected catalogue page.
    const std::size_t startingIndex = currentPage * pageSize;
    std::size_t endingIndex = startingIndex + pageSize;
    if (endingIndex > books.size()) {
        endingIndex = books.size();
    }

    printRecordPageInformation(currentPage, books.size(), pageSize);
    std::cout << std::left << std::setw(9) << "ID" << std::setw(32) << "Title"
              << std::setw(23) << "Author" << std::setw(18) << "Category"
              << std::right << std::setw(7) << "Avail" << std::setw(8)
              << "Loans" << '\n' << std::string(97, '-') << '\n';
    for (std::size_t index = startingIndex; index < endingIndex; ++index) {
        const Book* book = books[index];
        std::cout << std::left << std::setw(9) << book->id << std::setw(32)
                  << shortText(book->title, 31) << std::setw(23)
                  << shortText(book->author, 22) << std::setw(18)
                  << shortText(book->category, 17) << std::right << std::setw(7)
                  << copiesAvailableForGeneralBorrowing(data, book->id)
                  << std::setw(8) << book->borrowCount << '\n';
    }
}

void registrationScreen(LibraryData& data) {
    printHeading("MEMBER REGISTRATION");
    Member member;
    member.name = readValidName("Full name: ");
    member.phone =
        readValidPhone("Phone number (Example: +60123456789): ");
    member.email = readValidEmail("Email address: ");
    member.password = readValidPassword("Password (6-30 characters, no spaces): ");
    std::string confirmation;
    do {
        confirmation = readRequiredLine("Confirm password: ");
        if (member.password != confirmation) {
            std::cout << "Passwords do not match. Please enter the confirmation again.\n";
        }
    } while (member.password != confirmation);
    member.id = generateNextMemberId(data);
    const OperationResult result = registerMember(data, member);
    printResult(result);
    if (result.success) {
        std::cout << "Generated Member ID: " << member.id << "\n";
    }
    waitForEnter();
}

void searchBookScreen(const LibraryData& data) {
    printHeading("SEARCH AND VIEW BOOKS");
    std::cout << "Search by book ID, title, author or category.\n";
    std::cout << "Leave the keyword blank to show all active books.\nKeyword: ";
    std::string keyword;
    readConsoleLineOrExit(keyword);
    const bool availableOnly =
        readYesNo("Show only books currently available for borrowing?");

    const std::vector<const Book*> matches = searchBooks(data, keyword);
    std::vector<const Book*> filteredBooks;
    // READY reservation holds are excluded by the shared availability function.
    for (std::size_t index = 0; index < matches.size(); ++index) {
        if (!availableOnly ||
            copiesAvailableForGeneralBorrowing(data, matches[index]->id) > 0) {
            filteredBooks.push_back(matches[index]);
        }
    }

    if (filteredBooks.empty()) {
        std::cout << "No matching books found.\n";
        waitForEnter();
        return;
    }

    const std::size_t pageSize = RECORDS_PER_PAGE;
    const std::size_t totalPages =
        (filteredBooks.size() + pageSize - 1) / pageSize;
    std::size_t currentPage = 0;
    printBookPage(data, filteredBooks, currentPage, pageSize);

    while (selectAnotherPage(currentPage, totalPages)) {
        printBookPage(data, filteredBooks, currentPage, pageSize);
    }
}

void borrowBookScreen(LibraryData& data, const std::string& memberId) {
    printHeading("BORROW A BOOK");
    std::cout << "Search by book ID, title, author or category before selecting.\n";
    const std::string keyword = readRequiredLine("Search keyword: ");
    const std::vector<const Book*> matches = searchBooks(data, keyword);
    std::vector<const Book*> availableBooks;
    // Only books with an unreserved copy can be selected for borrowing.
    for (std::size_t index = 0; index < matches.size(); ++index) {
        if (copiesAvailableForGeneralBorrowing(data, matches[index]->id) > 0) {
            availableBooks.push_back(matches[index]);
        }
    }
    if (availableBooks.empty()) {
        std::cout << "No available books match that keyword.\n";
        waitForEnter();
        return;
    }
    printBookList(data, availableBooks);
    const std::string bookId = readValidIdentifier("Book ID to borrow: ");
    const Book* book = findBook(data, bookId);
    if (book == nullptr || !book->active ||
        copiesAvailableForGeneralBorrowing(data, bookId) <= 0) {
        std::cout << "This book is no longer available for borrowing.\n";
        waitForEnter();
        return;
    }
    const Date date = readDateNotInFuture("Borrowing date");
    const CheckoutDetails payment =
        readCheckoutDetails(BORROWING_FEE + REFUNDABLE_DEPOSIT);
    const OperationResult result = borrowBook(data, memberId, bookId, date,
                                               payment.method,
                                               payment.cashReceived);
    printResult(result);
    if (result.success) {
        printPaymentSuccess(payment);
        std::cout << "\nBORROWING RECEIPT\n"
                  << "Loan ID       : " << result.recordId << '\n'
                  << "Member ID     : " << memberId << '\n'
                  << "Book          : " << bookTitle(data, bookId) << '\n'
                  << "Amount paid   : RM" << std::fixed << std::setprecision(2)
                  << result.amount << '\n'
                  << "Deposit part  : RM" << result.secondaryAmount << '\n'
                  << "Method        : " << paymentMethodLabel(payment.method)
                  << '\n';
        if (payment.method == "CASH") {
            std::cout << "Cash received : RM" << payment.cashReceived << '\n'
                      << "Change        : RM" << payment.change << '\n';
        }
    }
    waitForEnter();
}

// Repeats until the member enters an active loan ID that belongs to their account.
Loan* readActiveLoanForReturn(LibraryData& data, const std::string& memberId) {
    while (true) {
        const int loanId = readIntInRange("Loan ID to return: ", 1, 999999);
        Loan* loan = findLoan(data, loanId);
        if (loan == nullptr || loan->status != LoanStatus::Active ||
            lowerCopy(loan->memberId) != lowerCopy(memberId)) {
            std::cout << "Invalid Loan ID. Please enter a valid loan ID.\n";
        } else {
            return loan;
        }
    }
}

// Repeats until the return date is valid, not future and not before the loan date.
Date readValidReturnDate(const Loan& loan) {
    while (true) {
        const Date returnDate = readDateNotInFuture("Return date");
        if (daysBetween(loan.borrowDate, returnDate) < 0) {
            std::cout << "Return date cannot be earlier than the loan date.\n";
        } else {
            return returnDate;
        }
    }
}

void returnBookScreen(LibraryData& data, const std::string& memberId) {
    printHeading("RETURN A BOOK");
    int activeCount = 0;
    std::cout << std::left << std::setw(8) << "Loan" << std::setw(34) << "Book"
              << std::setw(13) << "Borrowed" << "Due\n"
              << std::string(68, '-') << '\n';
    for (const Loan& loan : data.loans) {
        if (lowerCopy(loan.memberId) == lowerCopy(memberId) &&
            loan.status == LoanStatus::Active) {
            ++activeCount;
            std::cout << std::left << std::setw(8) << loan.id << std::setw(34)
                      << shortText(bookTitle(data, loan.bookId), 33)
                      << std::setw(13) << formatDate(loan.borrowDate)
                      << formatDate(loan.dueDate) << '\n';
        }
    }
    if (activeCount == 0) {
        std::cout << "No active loans found.\n";
        waitForEnter();
        return;
    }
    Loan* loan = readActiveLoanForReturn(data, memberId);
    const Date returnDate = readValidReturnDate(*loan);
    const bool goodCondition = readYesNo("Is the book in good condition?");
    printResult(returnBook(data, memberId, loan->id, returnDate, goodCondition));
    waitForEnter();
}

void reservationScreen(LibraryData& data, const std::string& memberId) {
    printHeading("BOOK RESERVATION - EXTRA FEATURE 1");
    std::cout << "Search for an unavailable title before placing a reservation.\n";
    const std::string keyword = readRequiredLine("Search keyword: ");
    const std::vector<const Book*> matches = searchBooks(data, keyword);
    // Show only books with no copy available to general borrowers.
    std::vector<const Book*> unavailableBooks;
    for (std::size_t index = 0; index < matches.size(); ++index) {
        if (copiesAvailableForGeneralBorrowing(data, matches[index]->id) <= 0) {
            unavailableBooks.push_back(matches[index]);
        }
    }
    printBookList(data, unavailableBooks);
    if (unavailableBooks.empty()) {
        std::cout << "No unavailable books match that keyword.\n";
        waitForEnter();
        return;
    }
    const std::string bookId =
        readValidIdentifier("Unavailable book ID to reserve: ");
    const Book* book = findBook(data, bookId);
    if (book == nullptr || !book->active ||
        copiesAvailableForGeneralBorrowing(data, bookId) > 0) {
        std::cout << "That active book is not currently unavailable for reservation.\n";
        waitForEnter();
        return;
    }
    const Date date = readDateNotInFuture("Reservation date");
    const CheckoutDetails payment = readCheckoutDetails(RESERVATION_FEE);
    const OperationResult result = reserveBook(data, memberId, bookId, date,
                                               payment.method,
                                               payment.cashReceived);
    printResult(result);
    if (result.success) {
        printPaymentSuccess(payment);
        if (payment.method == "CASH") {
            std::cout << "Cash received : RM" << std::fixed
                      << std::setprecision(2) << payment.cashReceived << '\n'
                      << "Change        : RM" << payment.change << '\n';
        }
    }
    waitForEnter();
}

void myReservationsScreen(LibraryData& data, const std::string& memberId) {
    printHeading("MY RESERVATIONS");
    std::vector<const Reservation*> memberReservations;
    int activeCount = 0;
    for (const Reservation& reservation : data.reservations) {
        if (lowerCopy(reservation.memberId) != lowerCopy(memberId)) {
            continue;
        }
        memberReservations.push_back(&reservation);
        if (reservation.status == ReservationStatus::Waiting ||
            reservation.status == ReservationStatus::Ready) {
            ++activeCount;
        }
    }
    if (memberReservations.empty()) {
        std::cout << "No reservation history found.\n";
        waitForEnter();
        return;
    }

    const std::size_t totalPages =
        numberOfPages(memberReservations.size(), RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, memberReservations.size(),
                                   RECORDS_PER_PAGE);
        std::cout << std::left << std::setw(8) << "ID" << std::setw(34)
                  << "Book" << std::setw(14) << "Reserved" << std::setw(14)
                  << "Estimate" << std::setw(12) << "Status" << "Notice\n"
                  << std::string(94, '-') << '\n';
        const std::size_t startingIndex = currentPage * RECORDS_PER_PAGE;
        std::size_t endingIndex = startingIndex + RECORDS_PER_PAGE;
        if (endingIndex > memberReservations.size()) {
            endingIndex = memberReservations.size();
        }
        for (std::size_t index = startingIndex; index < endingIndex; ++index) {
            const Reservation* reservation = memberReservations[index];
            std::cout << std::left << std::setw(8) << reservation->id
                      << std::setw(34)
                      << shortText(bookTitle(data, reservation->bookId), 33)
                      << std::setw(14)
                      << formatDate(reservation->reservationDate)
                      << std::setw(14)
                      << formatDate(reservation->estimatedAvailableDate)
                      << std::setw(12)
                      << reservationStatusText(reservation->status)
                      << (reservation->status == ReservationStatus::Ready
                              ? "PICK UP"
                              : "-")
                      << '\n';
        }
        showPage = selectAnotherPage(currentPage, totalPages);
    }

    if (activeCount > 0 &&
        readYesNo("Would you like to cancel an active reservation?")) {
        const int id = readIntInRange("Reservation ID: ", 1, 999999);
        printResult(cancelReservation(data, memberId, id));
    }
    waitForEnter();
}

void accountSummaryScreen(const LibraryData& data, const Member& member) {
    printHeading("MEMBER ACCOUNT SUMMARY");
    std::cout << "Member ID        : " << member.id << '\n'
              << "Name             : " << member.name << '\n'
              << "Phone            : " << member.phone << '\n'
              << "Email            : " << member.email << '\n'
              << "Active loans     : " << countActiveLoans(data, member.id) << '\n'
              << "Outstanding fine : RM" << std::fixed << std::setprecision(2)
              << member.outstandingFine << '\n';
    waitForEnter();
}

void memberBorrowingHistoryScreen(const LibraryData& data,
                                  const std::string& memberId) {
    printHeading("MY BORROWING HISTORY");
    std::vector<const Loan*> history;
    for (const Loan& loan : data.loans) {
        if (lowerCopy(loan.memberId) == lowerCopy(memberId)) {
            history.push_back(&loan);
        }
    }
    sortLoansNewestFirst(history);
    if (history.empty()) {
        std::cout << "No borrowing history found.\n";
        waitForEnter();
        return;
    }

    const std::size_t totalPages =
        numberOfPages(history.size(), RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, history.size(),
                                   RECORDS_PER_PAGE);
        std::cout << std::left << std::setw(8) << "Loan" << std::setw(31)
                  << "Book" << std::setw(13) << "Borrowed" << std::setw(13)
                  << "Due" << std::setw(13) << "Returned" << "Status\n"
                  << std::string(88, '-') << '\n';
        const std::size_t startingIndex = currentPage * RECORDS_PER_PAGE;
        std::size_t endingIndex = startingIndex + RECORDS_PER_PAGE;
        if (endingIndex > history.size()) {
            endingIndex = history.size();
        }
        for (std::size_t index = startingIndex; index < endingIndex; ++index) {
            const Loan* loan = history[index];
            std::cout << std::left << std::setw(8) << loan->id << std::setw(31)
                      << shortText(bookTitle(data, loan->bookId), 30)
                      << std::setw(13) << formatDate(loan->borrowDate)
                      << std::setw(13) << formatDate(loan->dueDate)
                      << std::setw(13) << formatDate(loan->returnDate)
                      << (loan->status == LoanStatus::Active ? "ACTIVE"
                                                            : "RETURNED")
                       << '\n';
        }
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void memberPaymentHistoryScreen(const LibraryData& data,
                                const std::string& memberId) {
    printHeading("MY PAYMENT HISTORY");
    std::vector<const Payment*> history;
    for (const Payment& payment : data.payments) {
        if (lowerCopy(payment.memberId) == lowerCopy(memberId)) {
            history.push_back(&payment);
        }
    }
    sortPaymentsNewestFirst(history);
    if (history.empty()) {
        std::cout << "No payment history found.\n";
        waitForEnter();
        return;
    }

    const std::size_t totalPages =
        numberOfPages(history.size(), RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, history.size(),
                                   RECORDS_PER_PAGE);
        std::cout << std::left << std::setw(8) << "Txn" << std::setw(20)
                  << "Type" << std::setw(17) << "Method" << std::setw(13)
                  << "Date" << std::right
                  << std::setw(11) << "Amount" << "  Note\n"
                  << std::string(80, '-') << '\n';
        const std::size_t startingIndex = currentPage * RECORDS_PER_PAGE;
        std::size_t endingIndex = startingIndex + RECORDS_PER_PAGE;
        if (endingIndex > history.size()) {
            endingIndex = history.size();
        }
        for (std::size_t index = startingIndex; index < endingIndex; ++index) {
            const Payment* payment = history[index];
            std::cout << std::left << std::setw(8) << payment->id
                      << std::setw(20)
                      << payment->type << std::setw(17)
                      << paymentMethodLabel(payment->method) << std::setw(13)
                      << formatDate(payment->date)
                      << std::right << std::setw(11) << std::fixed
                      << std::setprecision(2) << payment->amount << "  "
                      << payment->note << '\n';
        }
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void accountScreen(const LibraryData& data, const std::string& memberId) {
    const Member* member = findMember(data, memberId);
    if (member == nullptr) {
        std::cout << "Member record not found.\n";
        waitForEnter();
        return;
    }

    int choice = -1;
    do {
        printHeading("MEMBER ACCOUNT AND HISTORY");
        std::cout << "1. View account summary\n"
                  << "2. View borrowing history\n"
                  << "3. View payment history\n"
                  << "0. Return\n";
        choice = readIntInRange("Select an option: ", 0, 3);
        if (choice == 1) {
            accountSummaryScreen(data, *member);
        } else if (choice == 2) {
            memberBorrowingHistoryScreen(data, memberId);
        } else if (choice == 3) {
            memberPaymentHistoryScreen(data, memberId);
        }
    } while (choice != 0);
}

void finePaymentScreen(LibraryData& data, const std::string& memberId) {
    // The entered amount is limited to the member's current outstanding fine.
    printHeading("FINE PAYMENT");
    const Member* member = findMember(data, memberId);
    if (member == nullptr || member->outstandingFine < 0.005) {
        std::cout << "No outstanding fine is due.\n";
        waitForEnter();
        return;
    }
    std::cout << "Outstanding balance: RM" << std::fixed << std::setprecision(2)
              << member->outstandingFine << '\n';
    const double amount =
        readPositiveAmount("Payment amount: RM", member->outstandingFine);
    const CheckoutDetails payment = readCheckoutDetails(amount);
    const Date date = readDateNotInFuture("Payment date");
    const OperationResult result = payFine(data, memberId, amount, date,
                                            payment.method,
                                            payment.cashReceived);
    if (result.success) {
        std::cout << "\nPAYMENT SUCCESSFUL.\n"
                  << "Remaining fine: RM" << std::fixed << std::setprecision(2)
                  << result.secondaryAmount << ".\n";
        printPaymentSuccess(payment);
        std::cout << "\nPAYMENT RECEIPT\nTransaction ID: " << result.recordId
                  << "\nAmount paid   : RM" << std::fixed << std::setprecision(2)
                  << result.amount << "\nBalance       : RM" << result.secondaryAmount
                  << "\nMethod        : " << paymentMethodLabel(payment.method)
                  << '\n';
        if (payment.method == "CASH") {
            std::cout << "Cash received : RM" << payment.cashReceived << '\n'
                      << "Change        : RM" << payment.change << '\n';
        }
    } else {
        printResult(result);
    }
    waitForEnter();
}

void recommendationScreen(const LibraryData& data) {
    // This report focuses on the borrowing totals used for the ranking.
    printHeading("TOP BORROWED BOOKS - EXTRA FEATURE 2");
    const std::vector<const Book*> books = topBorrowedBooks(data, 5);
    std::cout << std::left << std::setw(7) << "Rank" << std::setw(9) << "ID"
              << std::setw(35) << "Title" << std::setw(22) << "Category"
              << std::right << std::setw(11) << "Borrowed" << '\n'
              << std::string(84, '-') << '\n';
    for (std::size_t index = 0; index < books.size(); ++index) {
        std::cout << std::left << std::setw(7) << index + 1 << std::setw(9)
                  << books[index]->id << std::setw(35)
                  << shortText(books[index]->title, 34) << std::setw(22)
                  << shortText(books[index]->category, 21) << std::right
                  << std::setw(11)
                  << borrowingHistoryCount(data, books[index]->id) << '\n';
    }
    if (books.empty()) {
        std::cout << "No borrowing history is available for recommendations.\n";
    }
    std::cout << "\nRecommendations are ranked from actual borrowing-history records.\n";
    waitForEnter();
}

void updateOwnProfileScreen(LibraryData& data, const std::string& memberId) {
    printHeading("UPDATE MY PROFILE");
    const Member* member = findMember(data, memberId);
    if (member == nullptr) {
        return;
    }
    std::cout << "Current name : " << member->name << "\nCurrent phone: "
              << member->phone << "\nCurrent email: " << member->email << '\n';
    const std::string name = readValidName("New full name: ");
    const std::string phone =
        readValidPhone("New phone (Example: +60123456789): ");
    const std::string email = readValidEmail("New email: ");
    printResult(updateMemberProfile(data, memberId, name, phone, email));
    waitForEnter();
}

void showReadyNotifications(LibraryData& data, const std::string& memberId) {
    const LibraryData original = data;
    bool notificationShown = false;
    for (Reservation& reservation : data.reservations) {
        if (lowerCopy(reservation.memberId) == lowerCopy(memberId) &&
            reservation.status == ReservationStatus::Ready &&
            !reservation.notified) {
            std::cout << "NOTICE: Reservation #" << reservation.id << " - "
                      << bookTitle(data, reservation.bookId)
                      << " is ready for pickup.\n";
            reservation.notified = true;
            notificationShown = true;
        }
    }
    if (notificationShown) {
        std::string saveMessage;
        if (!saveAllData(data, saveMessage)) {
            data = original;
            std::cout << "WARNING: Ready notification status could not be saved. "
                      << saveMessage << '\n';
        }
    }
}

void memberPortal(LibraryData& data, const std::string& memberId) {
    // This menu connects member actions to management, borrowing and extra features.
    int choice = -1;
    do {
        const Member* member = findMember(data, memberId);
        printHeading("MEMBER PORTAL - " +
                     (member == nullptr ? memberId : member->name));
        showReadyNotifications(data, memberId);
        std::cout << "1. Search and view books\n"
                  << "2. Borrow a book\n"
                  << "3. Return a book\n"
                  << "4. Reserve an unavailable book\n"
                  << "5. View or cancel my reservations\n"
                  << "6. View account and histories\n"
                  << "7. Pay outstanding fines\n"
                  << "8. View top borrowed recommendations\n"
                  << "9. Update my profile\n"
                  << "0. Logout\n";
        choice = readIntInRange("Select an option: ", 0, 9);
        switch (choice) {
            case 1:
                searchBookScreen(data);
                break;
            case 2:
                borrowBookScreen(data, memberId);
                break;
            case 3:
                returnBookScreen(data, memberId);
                break;
            case 4:
                reservationScreen(data, memberId);
                break;
            case 5:
                myReservationsScreen(data, memberId);
                break;
            case 6:
                accountScreen(data, memberId);
                break;
            case 7:
                finePaymentScreen(data, memberId);
                break;
            case 8:
                recommendationScreen(data);
                break;
            case 9:
                updateOwnProfileScreen(data, memberId);
                break;
            case 0:
                std::cout << "Logging out of the member portal.\n";
                break;
        }
    } while (choice != 0);
}

// Displays one search result set and returns true when another search is wanted.
bool showMemberSearchPages(const std::vector<const Member*>& members) {
    std::size_t currentPage = 0;
    const std::size_t totalPages =
        numberOfPages(members.size(), RECORDS_PER_PAGE);

    while (true) {
        if (members.empty()) {
            std::cout << "No matching active members found.\n";
        } else {
            printRecordPageInformation(currentPage, members.size(),
                                       RECORDS_PER_PAGE);
            std::cout << std::left << std::setw(10) << "ID" << std::setw(25)
                      << "Name" << std::setw(18) << "Phone" << "Email\n"
                      << std::string(80, '-') << '\n';
            const std::size_t startingIndex = currentPage * RECORDS_PER_PAGE;
            std::size_t endingIndex = startingIndex + RECORDS_PER_PAGE;
            if (endingIndex > members.size()) {
                endingIndex = members.size();
            }
            for (std::size_t index = startingIndex; index < endingIndex;
                 ++index) {
                const Member* member = members[index];
                std::cout << std::left << std::setw(10) << member->id
                          << std::setw(25) << shortText(member->name, 24)
                          << std::setw(18) << member->phone << member->email
                          << '\n';
            }
        }

        std::cout << "\n1. Next Page\n2. Previous Page\n"
                  << "3. Continue Searching\n4. Return\n";
        const int choice = readIntInRange("Select an option: ", 1, 4);
        if (choice == 1) {
            if (members.empty() || currentPage + 1 >= totalPages) {
                std::cout << "You are already on the last page.\n";
            } else {
                ++currentPage;
            }
        } else if (choice == 2) {
            if (members.empty() || currentPage == 0) {
                std::cout << "You are already on the first page.\n";
            } else {
                --currentPage;
            }
        } else if (choice == 3) {
            return true;
        } else {
            return false;
        }
    }
}

// Repeats librarian member searches without forcing a return to the main menu.
void memberSearchScreen(const LibraryData& data) {
    bool continueSearching = true;
    while (continueSearching) {
        printHeading("SEARCH MEMBERS");
        const std::string keyword =
            readRequiredLine("ID/name/phone/email: ");
        const std::vector<const Member*> matches = searchMembers(data, keyword);
        continueSearching = showMemberSearchPages(matches);
    }
}

void showMemberReportPages(const LibraryData& data) {
    const std::size_t recordCount = countMemberReportRecords(data);
    if (recordCount == 0) {
        printMemberReport(data, std::cout, 0, RECORDS_PER_PAGE);
        waitForEnter();
        return;
    }
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printMemberReport(data, std::cout, currentPage * RECORDS_PER_PAGE,
                          RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void showInventoryReportPages(const LibraryData& data) {
    const std::size_t recordCount = countInventoryReportRecords(data);
    if (recordCount == 0) {
        printInventoryReport(data, std::cout, 0, RECORDS_PER_PAGE);
        waitForEnter();
        return;
    }
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printInventoryReport(data, std::cout, currentPage * RECORDS_PER_PAGE,
                             RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void showBorrowingReportPages(const LibraryData& data,
                              const std::string& filter) {
    const std::size_t recordCount = countBorrowingReportRecords(data, filter);
    if (recordCount == 0) {
        printBorrowingReport(data, std::cout, filter, 0, RECORDS_PER_PAGE);
        waitForEnter();
        return;
    }
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printBorrowingReport(data, std::cout, filter,
                             currentPage * RECORDS_PER_PAGE,
                             RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void showOverdueReportPages(const LibraryData& data, const Date& reportDate) {
    const std::size_t recordCount =
        countOverdueReportRecords(data, reportDate);
    if (recordCount == 0) {
        printOverdueReport(data, reportDate, std::cout, 0, RECORDS_PER_PAGE);
        waitForEnter();
        return;
    }
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printOverdueReport(data, reportDate, std::cout,
                           currentPage * RECORDS_PER_PAGE, RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void showPaymentReportPages(const LibraryData& data,
                            const std::string& filter) {
    const std::size_t recordCount = countPaymentReportRecords(data, filter);
    if (recordCount == 0) {
        printPaymentReport(data, std::cout, filter, 0, RECORDS_PER_PAGE);
        waitForEnter();
        return;
    }
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printPaymentReport(data, std::cout, filter,
                           currentPage * RECORDS_PER_PAGE, RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

// Shows filtered loan records and returns true when the librarian wants a new filter.
bool showBorrowingTransactionPages(const LibraryData& data,
                                   const std::string& filter) {
    const std::size_t recordCount = countBorrowingReportRecords(data, filter);
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;

    while (true) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printBorrowingReport(data, std::cout, filter,
                             currentPage * RECORDS_PER_PAGE,
                             RECORDS_PER_PAGE);
        std::cout << "\n1. Next Page\n2. Previous Page\n"
                  << "3. Continue Viewing (apply another filter)\n"
                  << "4. Return\n";
        const int choice = readIntInRange("Select an option: ", 1, 4);
        if (choice == 1) {
            if (currentPage + 1 >= totalPages) {
                std::cout << "You are already on the last page.\n";
            } else {
                ++currentPage;
            }
        } else if (choice == 2) {
            if (currentPage == 0) {
                std::cout << "You are already on the first page.\n";
            } else {
                --currentPage;
            }
        } else if (choice == 3) {
            return true;
        } else {
            return false;
        }
    }
}

// Shows filtered payment records and returns true when the librarian wants a new filter.
bool showPaymentTransactionPages(const LibraryData& data,
                                 const std::string& filter) {
    const std::size_t recordCount = countPaymentReportRecords(data, filter);
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;

    while (true) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printPaymentReport(data, std::cout, filter,
                           currentPage * RECORDS_PER_PAGE,
                           RECORDS_PER_PAGE);
        std::cout << "\n1. Next Page\n2. Previous Page\n"
                  << "3. Continue Viewing (apply another filter)\n"
                  << "4. Return\n";
        const int choice = readIntInRange("Select an option: ", 1, 4);
        if (choice == 1) {
            if (currentPage + 1 >= totalPages) {
                std::cout << "You are already on the last page.\n";
            } else {
                ++currentPage;
            }
        } else if (choice == 2) {
            if (currentPage == 0) {
                std::cout << "You are already on the first page.\n";
            } else {
                --currentPage;
            }
        } else if (choice == 3) {
            return true;
        } else {
            return false;
        }
    }
}

void showCategoryStatisticsPages(const LibraryData& data) {
    const std::size_t recordCount = countCategoryStatisticsRecords(data);
    if (recordCount == 0) {
        printCategoryStatistics(data, std::cout, 0, RECORDS_PER_PAGE);
        waitForEnter();
        return;
    }
    const std::size_t totalPages =
        numberOfPages(recordCount, RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, recordCount, RECORDS_PER_PAGE);
        printCategoryStatistics(data, std::cout,
                                currentPage * RECORDS_PER_PAGE,
                                RECORDS_PER_PAGE);
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void memberManagementMenu(LibraryData& data) {
    int choice = -1;
    do {
        printHeading("LIBRARIAN - MEMBER MANAGEMENT");
        std::cout << "1. Register a member\n2. Search members\n"
                  << "3. Update a member\n4. Delete a member\n"
                  << "5. Display member report\n0. Return\n";
        choice = readIntInRange("Select an option: ", 0, 5);
        if (choice == 1) {
            registrationScreen(data);
        } else if (choice == 2) {
            memberSearchScreen(data);
        } else if (choice == 3) {
            const std::string id = readValidIdentifier("Member ID: ");
            const Member* member = findMember(data, id);
            if (member == nullptr || !member->active) {
                std::cout << "Active member not found.\n";
                waitForEnter();
                continue;
            }
            const std::string name = readValidName("New full name: ");
            const std::string phone =
                readValidPhone("New phone (Example: +60123456789): ");
            const std::string email = readValidEmail("New email: ");
            printResult(updateMemberProfile(data, id, name, phone, email));
            waitForEnter();
        } else if (choice == 4) {
            const std::string id = readRequiredLine("Member ID to delete: ");
            if (readYesNo("Confirm deactivation of this member?")) {
                printResult(deleteMember(data, id));
            }
            waitForEnter();
        } else if (choice == 5) {
            showMemberReportPages(data);
        }
    } while (choice != 0);
}

void bookManagementMenu(LibraryData& data) {
    int choice = -1;
    do {
        printHeading("LIBRARIAN - BOOK CATALOGUE AND INVENTORY");
        std::cout << "1. Add a book\n2. Search books\n3. Update a book\n"
                  << "4. Delete a book\n5. Display inventory report\n0. Return\n";
        choice = readIntInRange("Select an option: ", 0, 5);
        if (choice == 1) {
            Book book;
            book.title = readBookText("Title: ", 100, "title");
            book.author = readBookText("Author: ", 60, "author");
            book.category = readBookCategory();
            book.totalCopies = readIntInRange("Number of copies: ", 1, 999);
            book.id = generateNextBookId(data);
            const OperationResult result = addBook(data, book);
            printResult(result);
            if (result.success) {
                std::cout << "Generated Book ID: " << book.id << "\n";
            }
            waitForEnter();
        } else if (choice == 2) {
            std::cout << "Keyword (blank shows all): ";
            std::string keyword;
            readConsoleLineOrExit(keyword);
            const std::vector<const Book*> matches = searchBooks(data, keyword);
            printBookList(data, matches);
            if (matches.empty()) {
                waitForEnter();
            }
        } else if (choice == 3) {
            const std::string id = readValidIdentifier("Book ID: ");
            const Book* book = findBook(data, id);
            if (book == nullptr || !book->active) {
                std::cout << "Active book not found.\n";
                waitForEnter();
                continue;
            }
            const std::string title = readBookText("New title: ", 100, "title");
            const std::string author = readBookText("New author: ", 60, "author");
            const std::string category = readBookCategory();
            const int copies = readIntInRange("New total copies: ", 1, 999);
            printResult(updateBook(data, id, title, author, category, copies));
            waitForEnter();
        } else if (choice == 4) {
            const std::string id = readValidIdentifier("Book ID to delete: ");
            const Book* book = findBook(data, id);
            if (book == nullptr || !book->active) {
                std::cout << "Active book not found.\n";
                waitForEnter();
                continue;
            }
            if (readYesNo("Confirm deactivation of this book?")) {
                printResult(deleteBook(data, id));
            }
            waitForEnter();
        } else if (choice == 5) {
            showInventoryReportPages(data);
        }
    } while (choice != 0);
}

void printReservationQueue(const LibraryData& data) {
    if (data.reservations.empty()) {
        std::cout << "No reservation records found.\n";
        waitForEnter();
        return;
    }

    const std::size_t totalPages =
        numberOfPages(data.reservations.size(), RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool showPage = true;
    while (showPage) {
        printRecordPageInformation(currentPage, data.reservations.size(),
                                   RECORDS_PER_PAGE);
        std::cout << "\nRESERVATION QUEUE\n" << std::left << std::setw(8)
                  << "ID" << std::setw(11) << "Book" << std::setw(12)
                  << "Member" << std::setw(13) << "Reserved" << std::setw(13)
                  << "Estimate" << std::setw(12) << "Status" << "Notified\n"
                  << std::string(79, '-') << '\n';
        const std::size_t startingIndex = currentPage * RECORDS_PER_PAGE;
        std::size_t endingIndex = startingIndex + RECORDS_PER_PAGE;
        if (endingIndex > data.reservations.size()) {
            endingIndex = data.reservations.size();
        }
        for (std::size_t index = startingIndex; index < endingIndex; ++index) {
            const Reservation& reservation = data.reservations[index];
            std::cout << std::left << std::setw(8) << reservation.id
                      << std::setw(11) << reservation.bookId << std::setw(12)
                      << reservation.memberId << std::setw(13)
                      << formatDate(reservation.reservationDate) << std::setw(13)
                      << formatDate(reservation.estimatedAvailableDate)
                      << std::setw(12)
                      << reservationStatusText(reservation.status)
                      << (reservation.notified ? "YES" : "NO") << '\n';
        }
        showPage = selectAnotherPage(currentPage, totalPages);
    }
}

void transactionMonitoringMenu(const LibraryData& data) {
    // Librarians can inspect filtered loan, reservation and payment records.
    int choice = -1;
    do {
        printHeading("LIBRARIAN - TRANSACTION MONITORING");
        std::cout << "1. View borrowing and return records\n"
                  << "2. View reservation queue\n3. View payment transactions\n"
                  << "0. Return\n";
        choice = readIntInRange("Select an option: ", 0, 3);
        switch (choice) {
            case 1: {
                bool continueViewing = true;
                while (continueViewing) {
                    std::cout << "Filter by loan/member/book/status "
                                 "(blank shows all): ";
                    std::string filter;
                    readConsoleLineOrExit(filter);
                    continueViewing = showBorrowingTransactionPages(data, filter);
                }
                break;
            }
            case 2:
                printReservationQueue(data);
                break;
            case 3: {
                bool continueViewing = true;
                while (continueViewing) {
                    std::cout << "Filter by transaction/member/type/date/note "
                                 "(blank shows all): ";
                    std::string filter;
                    readConsoleLineOrExit(filter);
                    continueViewing = showPaymentTransactionPages(data, filter);
                }
                break;
            }
        }
    } while (choice != 0);
}

bool prepareReportFileName(const std::string& enteredName,
                           std::string& fileName) {
    if (!enteredName.empty() && enteredName[enteredName.size() - 1] == ' ') {
        return false;
    }
    fileName = trim(enteredName);
    if (fileName.empty() || fileName.find("..") != std::string::npos ||
        fileName.find('/') != std::string::npos ||
        fileName.find('\\') != std::string::npos) {
        return false;
    }

    if (fileName.size() < 4 ||
        lowerCopy(fileName.substr(fileName.size() - 4)) != ".txt") {
        fileName += ".txt";
    }

    if (fileName.size() > 60) {
        return false;
    }
    const std::string baseName = fileName.substr(0, fileName.size() - 4);
    if (baseName.empty() || baseName[baseName.size() - 1] == ' ' ||
        baseName[baseName.size() - 1] == '.') {
        return false;
    }
    for (std::size_t index = 0; index < baseName.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(baseName[index]);
        if (std::isalnum(character) == 0 && baseName[index] != ' ' &&
            baseName[index] != '_' && baseName[index] != '-') {
            return false;
        }
    }

    const std::string lowerBaseName = lowerCopy(baseName);
    if (lowerBaseName == "con" || lowerBaseName == "prn" ||
        lowerBaseName == "aux" || lowerBaseName == "nul") {
        return false;
    }
    if (lowerBaseName.size() == 4 &&
        (lowerBaseName.substr(0, 3) == "com" ||
         lowerBaseName.substr(0, 3) == "lpt") &&
        lowerBaseName[3] >= '1' && lowerBaseName[3] <= '9') {
        return false;
    }
    return true;
}

// Inspects a root report path without following links outside the workspace.
bool inspectReportFile(const std::string& fileName, bool& exists,
                       bool& regularFile, std::string& message) {
    const fs::path path = fileName;
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    if (error) {
        if (error == std::errc::no_such_file_or_directory) {
            exists = false;
            regularFile = false;
            return true;
        }
        message = "Unable to inspect report file " + fileName + ": " +
                  error.message() + ".";
        return false;
    }
    exists = fs::exists(status);
    regularFile = exists && fs::is_regular_file(status);
    return true;
}

std::string reportModifiedTime(const fs::directory_entry& entry) {
    std::error_code error;
    const fs::file_time_type fileTime = entry.last_write_time(error);
    if (error) {
        return "Unavailable";
    }

    const std::chrono::system_clock::time_point systemTime =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - fs::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    const std::time_t timeValue =
        std::chrono::system_clock::to_time_t(systemTime);
    const std::tm* localTime = std::localtime(&timeValue);
    if (localTime == nullptr) {
        return "Unavailable";
    }

    std::ostringstream formatted;
    formatted << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");
    return formatted.str();
}

// Reads regular root .txt files and keeps their display times aligned.
bool collectGeneratedReports(std::vector<std::string>& fileNames,
                             std::vector<std::string>& modifiedTimes,
                             std::string& message) {
    fileNames.clear();
    modifiedTimes.clear();
    std::error_code error;
    fs::directory_iterator current(fs::path("."), error);
    const fs::directory_iterator end;
    if (error) {
        message = "Unable to list generated reports: " + error.message() + ".";
        return false;
    }

    while (current != end) {
        const fs::directory_entry entry = *current;
        const fs::file_status status = entry.symlink_status(error);
        if (error) {
            message = "Unable to inspect report entry " +
                      entry.path().filename().string() + ": " +
                      error.message() + ".";
            return false;
        }
        if (fs::is_regular_file(status) &&
            lowerCopy(entry.path().extension().string()) == ".txt") {
            fileNames.push_back(entry.path().filename().string());
            modifiedTimes.push_back(reportModifiedTime(entry));
        }
        current.increment(error);
        if (error) {
            message = "Unable to continue listing generated reports: " +
                      error.message() + ".";
            return false;
        }
    }

    // A simple selection sort keeps this feature easy to explain.
    for (std::size_t left = 0; left < fileNames.size(); ++left) {
        std::size_t smallest = left;
        for (std::size_t right = left + 1; right < fileNames.size(); ++right) {
            if (lowerCopy(fileNames[right]) < lowerCopy(fileNames[smallest])) {
                smallest = right;
            }
        }
        if (smallest != left) {
            const std::string savedName = fileNames[left];
            const std::string savedTime = modifiedTimes[left];
            fileNames[left] = fileNames[smallest];
            modifiedTimes[left] = modifiedTimes[smallest];
            fileNames[smallest] = savedName;
            modifiedTimes[smallest] = savedTime;
        }
    }
    return true;
}

void printGeneratedReportListPage(
    const std::vector<std::string>& fileNames,
    const std::vector<std::string>& modifiedTimes, std::size_t currentPage) {
    printRecordPageInformation(currentPage, fileNames.size(), RECORDS_PER_PAGE);
    std::cout << "\n" << std::left << std::setw(44) << "Filename"
              << "Last modified\n" << std::string(65, '-') << '\n';
    const std::size_t startingIndex = currentPage * RECORDS_PER_PAGE;
    std::size_t endingIndex = startingIndex + RECORDS_PER_PAGE;
    if (endingIndex > fileNames.size()) {
        endingIndex = fileNames.size();
    }
    for (std::size_t index = startingIndex; index < endingIndex; ++index) {
        std::cout << std::left << std::setw(44)
                  << shortText(fileNames[index], 42) << modifiedTimes[index]
                  << '\n';
    }
}

void listGeneratedReportsScreen() {
    printHeading("LIST GENERATED REPORTS");
    std::vector<std::string> fileNames;
    std::vector<std::string> modifiedTimes;
    std::string message;
    if (!collectGeneratedReports(fileNames, modifiedTimes, message)) {
        std::cout << message << '\n';
        waitForEnter();
        return;
    }
    if (fileNames.empty()) {
        std::cout << "No generated text reports found.\n";
        waitForEnter();
        return;
    }

    const std::size_t totalPages =
        numberOfPages(fileNames.size(), RECORDS_PER_PAGE);
    std::size_t currentPage = 0;
    bool continueViewing = true;
    while (continueViewing) {
        printGeneratedReportListPage(fileNames, modifiedTimes, currentPage);
        if (totalPages > 1) {
            continueViewing = selectAnotherPage(currentPage, totalPages);
        } else {
            waitForEnter();
            continueViewing = false;
        }
    }
}

void renameGeneratedReportScreen() {
    printHeading("RENAME A GENERATED REPORT");
    const std::string oldEnteredName = readRequiredLine("Current filename: ");
    const std::string newEnteredName = readRequiredLine("New filename: ");
    std::string oldFileName;
    std::string newFileName;
    if (!prepareReportFileName(oldEnteredName, oldFileName) ||
        !prepareReportFileName(newEnteredName, newFileName)) {
        std::cout << "Invalid report filename. Use up to 60 letters, numbers, "
                     "spaces, underscores or hyphens.\n";
        waitForEnter();
        return;
    }

    bool oldExists = false;
    bool oldRegular = false;
    std::string message;
    if (!inspectReportFile(oldFileName, oldExists, oldRegular, message)) {
        std::cout << message << '\n';
        waitForEnter();
        return;
    }
    if (!oldExists || !oldRegular) {
        std::cout << "The original report does not exist as a regular file.\n";
        waitForEnter();
        return;
    }

    bool newExists = false;
    bool newRegular = false;
    if (!inspectReportFile(newFileName, newExists, newRegular, message)) {
        std::cout << message << '\n';
        waitForEnter();
        return;
    }
    if (newExists) {
        std::cout << "The target report filename already exists.\n";
        waitForEnter();
        return;
    }

    std::error_code error;
    fs::rename(fs::path(oldFileName), fs::path(newFileName), error);
    if (error) {
        std::cout << "Unable to rename the report: " << error.message()
                  << ".\n";
    } else {
        std::cout << "Report renamed successfully from " << oldFileName
                  << " to " << newFileName << ".\n";
    }
    waitForEnter();
}

void deleteGeneratedReportScreen() {
    printHeading("DELETE A GENERATED REPORT");
    const std::string enteredName = readRequiredLine("Report filename: ");
    std::string fileName;
    if (!prepareReportFileName(enteredName, fileName)) {
        std::cout << "Invalid report filename. Use up to 60 letters, numbers, "
                     "spaces, underscores or hyphens.\n";
        waitForEnter();
        return;
    }

    bool exists = false;
    bool regularFile = false;
    std::string message;
    if (!inspectReportFile(fileName, exists, regularFile, message)) {
        std::cout << message << '\n';
        waitForEnter();
        return;
    }
    if (!exists || !regularFile) {
        std::cout << "The report does not exist as a regular file.\n";
        waitForEnter();
        return;
    }

    std::cout << "Report to delete: " << fileName << '\n';
    if (!readYesNo("Delete this exact report permanently?")) {
        std::cout << "Report deletion cancelled.\n";
        waitForEnter();
        return;
    }

    std::error_code error;
    const bool removed = fs::remove(fs::path(fileName), error);
    if (error || !removed) {
        std::cout << "Unable to delete the report";
        if (error) {
            std::cout << ": " << error.message();
        }
        std::cout << ".\n";
    } else {
        std::cout << "Report deleted successfully: " << fileName << ".\n";
    }
    waitForEnter();
}

void printReportPage(const std::vector<std::string>& lines,
                     const std::string& fileName, std::size_t currentPage,
                     std::size_t pageSize) {
    if (pageSize == 0) {
        return;
    }

    std::size_t totalPages = 1;
    if (!lines.empty()) {
        totalPages = (lines.size() + pageSize - 1) / pageSize;
    }
    if (currentPage >= totalPages) {
        return;
    }

    const std::size_t startingIndex = currentPage * pageSize;
    std::size_t endingIndex = startingIndex + pageSize;
    if (endingIndex > lines.size()) {
        endingIndex = lines.size();
    }

    std::cout << "\nReport: " << fileName << "\nPage " << currentPage + 1
              << " of " << totalPages << "\n"
              << std::string(78, '-') << '\n';
    for (std::size_t index = startingIndex; index < endingIndex; ++index) {
        std::cout << lines[index] << '\n';
    }
    if (lines.empty()) {
        std::cout << "(The report file is empty.)\n";
    }
}

void generateSelectedReportScreen(const LibraryData& data) {
    printHeading("GENERATE A SELECTED REPORT");
    std::cout << "1. Member Account Report\n"
              << "2. Book Inventory Report\n"
              << "3. Borrowing and Return Report\n"
              << "4. Overdue Report\n"
              << "5. Payment Transaction Report\n"
              << "6. Category Statistics\n"
              << "0. Cancel\n";
    const int reportType = readIntInRange("Select a report: ", 0, 6);
    if (reportType == 0) {
        return;
    }

    const std::string reportTitle = readRequiredLine("Custom report title: ");
    std::string filter;
    if (reportType == 3) {
        std::cout << "Filter by loan/member/book/status "
                     "(blank exports all): ";
        readConsoleLineOrExit(filter);
    } else if (reportType == 5) {
        std::cout << "Filter by transaction/member/type/date/note "
                     "(blank exports all): ";
        readConsoleLineOrExit(filter);
    }
    std::cout << "Report filename: ";
    std::string enteredName;
    readConsoleLineOrExit(enteredName);
    std::string fileName;
    if (!prepareReportFileName(enteredName, fileName)) {
        std::cout << "Invalid report filename. Use up to 60 letters, numbers, "
                     "spaces, underscores or hyphens.\n";
        waitForEnter();
        return;
    }

    // Check before opening an output stream so an existing report is protected.
    bool reportExists = false;
    bool regularFile = false;
    std::string inspectionMessage;
    if (!inspectReportFile(fileName, reportExists, regularFile,
                           inspectionMessage)) {
        std::cout << inspectionMessage << '\n';
        waitForEnter();
        return;
    }
    if (reportExists && !regularFile) {
        std::cout << "The report path exists but is not a regular file.\n";
        waitForEnter();
        return;
    }
    if (reportExists) {
        std::cout << "A report with this filename already exists.\n";
        if (!readYesNo("Replace the existing report?")) {
            std::cout << "Report generation cancelled.\n";
            waitForEnter();
            return;
        }
    }

    Date reportDate = todayLocal();
    if (reportType == 4) {
        reportDate = readDate("Report as-of date");
    }

    std::string message;
    const bool generated = exportSelectedReport(
        data, reportType, reportDate, reportTitle, filter, fileName, message);
    std::cout << (generated ? "\nSUCCESS: " : "\nERROR: ") << message << '\n';
    waitForEnter();
}

void viewGeneratedReportScreen() {
    printHeading("VIEW A GENERATED REPORT");
    std::cout << "Report filename: ";
    std::string enteredName;
    readConsoleLineOrExit(enteredName);
    std::string fileName;
    if (!prepareReportFileName(enteredName, fileName)) {
        std::cout << "Invalid report filename. Use up to 60 letters, numbers, "
                     "spaces, underscores or hyphens.\n";
        waitForEnter();
        return;
    }

    bool reportExists = false;
    bool regularFile = false;
    std::string inspectionMessage;
    if (!inspectReportFile(fileName, reportExists, regularFile,
                           inspectionMessage) ||
        !reportExists || !regularFile) {
        std::cout << "Report file not found or cannot be opened.\n";
        waitForEnter();
        return;
    }

    std::ifstream reportFile(fileName);
    if (!reportFile.is_open()) {
        std::cout << "Report file not found or cannot be opened.\n";
        waitForEnter();
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(reportFile, line)) {
        lines.push_back(line);
    }
    reportFile.close();

    const std::size_t pageSize = 25;
    std::size_t totalPages = 1;
    if (!lines.empty()) {
        totalPages = (lines.size() + pageSize - 1) / pageSize;
    }
    std::size_t currentPage = 0;
    printReportPage(lines, fileName, currentPage, pageSize);

    bool closeReport = false;
    while (!closeReport) {
        std::cout << "\nN = Next page | P = Previous page | 0 = Close report\n"
                  << "Command: ";
        std::string command;
        readConsoleLineOrExit(command);
        command = lowerCopy(trim(command));

        if (command == "n") {
            if (currentPage + 1 >= totalPages) {
                std::cout << "You are already on the last page.\n";
            } else {
                ++currentPage;
                printReportPage(lines, fileName, currentPage, pageSize);
            }
        } else if (command == "p") {
            if (currentPage == 0) {
                std::cout << "You are already on the first page.\n";
            } else {
                --currentPage;
                printReportPage(lines, fileName, currentPage, pageSize);
            }
        } else if (command == "0") {
            closeReport = true;
        } else {
            std::cout << "Invalid command. Enter N, P or 0.\n";
        }
    }
}

void reportManagementMenu(const LibraryData& data) {
    int choice = -1;
    do {
        printHeading("LIBRARIAN - REPORT MANAGEMENT");
        std::cout << "1. Generate a selected report\n"
                  << "2. View a generated report\n"
                  << "3. List generated reports\n"
                  << "4. Rename a generated report\n"
                  << "5. Delete a generated report\n"
                  << "0. Return\n";
        choice = readIntInRange("Select an option: ", 0, 5);
        if (choice == 1) {
            generateSelectedReportScreen(data);
        } else if (choice == 2) {
            viewGeneratedReportScreen();
        } else if (choice == 3) {
            listGeneratedReportsScreen();
        } else if (choice == 4) {
            renameGeneratedReportScreen();
        } else if (choice == 5) {
            deleteGeneratedReportScreen();
        }
    } while (choice != 0);
}

void reportsMenu(const LibraryData& data) {
    int choice = -1;
    do {
        printHeading("LIBRARIAN - REPORTS AND STATISTICS");
        std::cout << "1. Member report\n2. Book inventory report\n"
                  << "3. Borrowing report\n4. Overdue report\n"
                  << "5. Payment report\n6. Category statistics\n"
                  << "7. Top borrowed books\n"
                  << "8. Report management\n0. Return\n";
        choice = readIntInRange("Select an option: ", 0, 8);
        switch (choice) {
            case 1:
                showMemberReportPages(data);
                continue;
            case 2:
                showInventoryReportPages(data);
                continue;
            case 3: {
                std::cout << "Filter by loan/member/book/status "
                             "(blank shows all): ";
                std::string filter;
                readConsoleLineOrExit(filter);
                showBorrowingReportPages(data, filter);
                continue;
            }
            case 4: {
                const Date reportDate = readDate("Report date");
                showOverdueReportPages(data, reportDate);
                continue;
            }
            case 5: {
                std::cout << "Filter by transaction/member/type/date/note "
                             "(blank shows all): ";
                std::string filter;
                readConsoleLineOrExit(filter);
                showPaymentReportPages(data, filter);
                continue;
            }
            case 6:
                showCategoryStatisticsPages(data);
                continue;
            case 7:
                recommendationScreen(data);
                continue;
            case 8:
                reportManagementMenu(data);
                continue;
        }
    } while (choice != 0);
}

void librarianPortal(LibraryData& data) {
    // The librarian menu connects the four main assignment modules.
    int choice = -1;
    do {
        printHeading("LIBRARIAN PORTAL");
        std::cout << "1. Member management\n2. Book catalogue and inventory\n"
                  << "3. Transaction monitoring\n4. Reports and statistics\n"
                  << "0. Logout\n";
        choice = readIntInRange("Select an option: ", 0, 4);
        switch (choice) {
            case 1:
                memberManagementMenu(data);
                break;
            case 2:
                bookManagementMenu(data);
                break;
            case 3:
                transactionMonitoringMenu(data);
                break;
            case 4:
                reportsMenu(data);
                break;
            case 0:
                std::cout << "Logging out of the librarian portal.\n";
                break;
        }
    } while (choice != 0);
}

void memberLoginScreen(LibraryData& data) {
    printHeading("MEMBER LOGIN");
    for (int attempt = 1; attempt <= 3; ++attempt) {
        const std::string id = readRequiredLine("Member ID: ");
        const std::string password = readRequiredLine("Password: ");
        if (authenticateMember(data, id, password)) {
            std::cout << "Login successful.\n";
            memberPortal(data, findMember(data, id)->id);
            return;
        }
        std::cout << "Invalid member ID or password. Attempts remaining: "
                  << 3 - attempt << "\n";
    }
    std::cout << "Login cancelled after three unsuccessful attempts.\n";
    waitForEnter();
}

void librarianLoginScreen(LibraryData& data) {
    printHeading("LIBRARIAN LOGIN");
    for (int attempt = 1; attempt <= 3; ++attempt) {
        const std::string id = readRequiredLine("Librarian ID: ");
        const std::string password = readRequiredLine("Password: ");
        if (authenticateLibrarian(id, password)) {
            std::cout << "Login successful.\n";
            librarianPortal(data);
            return;
        }
        std::cout << "Invalid librarian credentials. Attempts remaining: "
                  << 3 - attempt << "\n";
    }
    std::cout << "Login cancelled after three unsuccessful attempts.\n";
    waitForEnter();
}

}  // namespace

void runConsoleApplication(LibraryData& data) {
    // The same LibraryData object is passed through every menu and module.
    printHeading("TAR UMT LIBRARY MANAGEMENT SYSTEM");
    std::cout << "A role-based service platform for members and librarians.\n"
              << "It manages accounts, inventory, borrowing, returns, reservations,\n"
              << "fine payments, recommendations and statistical reports.\n";
    waitForEnter("Press Enter to open the main menu...");

    int choice = -1;
    do {
        printHeading("MAIN MENU");
        std::cout << "1. Member login\n2. Librarian login\n"
                  << "3. Register as a new member\n0. Exit\n";
        choice = readIntInRange("Select an option: ", 0, 3);
        switch (choice) {
            case 1:
                memberLoginScreen(data);
                break;
            case 2:
                librarianLoginScreen(data);
                break;
            case 3:
                registrationScreen(data);
                break;
            case 0:
                std::cout << "\nThank you for using the Library Management System.\n"
                          << "All changes have been saved. Goodbye!\n";
                break;
        }
    } while (choice != 0);
}

}  // namespace lms
