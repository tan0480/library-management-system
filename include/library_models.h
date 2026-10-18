// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Defines the constants and record structures shared by all modules.

#ifndef LIBRARY_MODELS_H
#define LIBRARY_MODELS_H

#include <string>
#include <vector>

namespace lms {

// These shared rules keep calculations consistent across all modules.
constexpr int LOAN_PERIOD_DAYS = 14;
constexpr int MAX_ACTIVE_LOANS = 5;
constexpr double OVERDUE_FINE_PER_DAY = 0.50;
constexpr double BORROWING_FEE = 2.00;
constexpr double RESERVATION_FEE = 1.00;
constexpr double REFUNDABLE_DEPOSIT = 20.00;

// Stores one calendar date used by loans, reservations and payments.
struct Date {
    int year = 0;
    int month = 0;
    int day = 0;
};

enum class LoanStatus { Active, Returned };
enum class ReservationStatus { Waiting, Ready, Fulfilled, Cancelled };

// Stores a member account and its current outstanding fine.
struct Member {
    std::string id;
    std::string password;
    std::string name;
    std::string phone;
    std::string email;
    bool active = true;
    double outstandingFine = 0.0;
};

// Stores catalogue details and physical inventory quantities.
struct Book {
    std::string id;
    std::string title;
    std::string author;
    std::string category;
    int totalCopies = 0;
    int availableCopies = 0;
    int borrowCount = 0;
    bool active = true;
};

// Stores one borrowing transaction from borrowing through return.
struct Loan {
    int id = 0;
    std::string memberId;
    std::string bookId;
    Date borrowDate;
    Date dueDate;
    Date returnDate;
    LoanStatus status = LoanStatus::Active;
    double borrowingFee = 0.0;
    double depositPaid = 0.0;
    double fineCharged = 0.0;
    double depositRefunded = 0.0;
    bool returnedInGoodCondition = true;
};

// Stores one FIFO queue entry and its current reservation state.
struct Reservation {
    int id = 0;
    std::string memberId;
    std::string bookId;
    Date reservationDate;
    Date estimatedAvailableDate;
    ReservationStatus status = ReservationStatus::Waiting;
    double reservationFee = 0.0;
    bool notified = false;
};

// Stores one fee, deposit, refund, fine charge or fine payment.
struct Payment {
    int id = 0;
    std::string memberId;
    std::string type;
    // Stores how a member paid. Older saved records use NOT_RECORDED.
    std::string method = "NOT_RECORDED";
    double amount = 0.0;
    Date date;
    int referenceId = 0;
    std::string note;
};

// Combines all shared vectors so every module works with the same records.
struct LibraryData {
    std::vector<Member> members;
    std::vector<Book> books;
    std::vector<Loan> loans;
    std::vector<Reservation> reservations;
    std::vector<Payment> payments;
    int nextLoanId = 1;
    int nextReservationId = 1;
    int nextPaymentId = 1;
    std::string dataDirectory;
};

// Returns an operation status, message and optional receipt values to the UI.
struct OperationResult {
    bool success = false;
    std::string message;
    int recordId = 0;
    double amount = 0.0;
    double secondaryAmount = 0.0;
};

}  // namespace lms

#endif
