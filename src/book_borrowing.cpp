// Modules 2 and 3: Book Borrowing, Returns, Fines and Payments
// Contributors:
// - Tan Yi Heng (2507419): borrowing, returns and inventory updates
// - Cheah Yong Shun (2505229): overdue fines and fine-payment integration

#include "book_borrowing.h"

#include "additional_functionalities.h"
#include "date_functions.h"
#include "record_search.h"
#include "storage.h"
#include "string_functions.h"
#include "payment_functions.h"

#include <cmath>
#include <iomanip>
#include <sstream>

using namespace std;

namespace lms {
namespace {

OperationResult circulationError(const std::string& message) {
    return OperationResult{false, message};
}

bool sameText(const std::string& left, const std::string& right) {
    return lowerCopy(left) == lowerCopy(right);
}

bool validCheckoutPayment(const std::string& method, double cashReceived,
                          double amountDue, std::string& error) {
    if (!isCustomerPaymentMethod(method)) {
        error = "A valid payment method must be selected.";
        return false;
    }
    if (method == "CASH" &&
        (!std::isfinite(cashReceived) || cashReceived + 0.005 < amountDue)) {
        error = "Cash received is less than the amount due.";
        return false;
    }
    return true;
}

std::string paymentNote(const std::string& description,
                        const std::string& method, double cashReceived,
                        double amountDue) {
    std::ostringstream note;
    note << description << "; paid by " << paymentMethodLabel(method);
    if (method == "CASH") {
        note << "; cash received RM" << std::fixed << std::setprecision(2)
             << cashReceived << "; change RM" << cashReceived - amountDue;
    }
    return note.str();
}

}  // namespace

// Contribution: Tan Yi Heng (2507419)
// Counts active loans so the member borrowing limit can be enforced.
int countActiveLoans(const LibraryData& data, const std::string& memberId) {
    int count = 0;
    for (const Loan& loan : data.loans) {
        if (sameText(loan.memberId, memberId) &&
            loan.status == LoanStatus::Active) {
            ++count;
        }
    }
    return count;
}

// Contribution: Tan Yi Heng (2507419)
// Validates a borrowing request and updates loans, inventory and payments.
OperationResult borrowBook(LibraryData& data, const std::string& memberId,
                           const std::string& bookId, const Date& borrowDate,
                           const std::string& paymentMethod,
                           double cashReceived) {
    // Validate the date, linked records, fine balance and borrowing limit first.
    if (!isValidDate(borrowDate)) {
        return circulationError("Borrowing date is invalid.");
    }
    const Date dueDate = addDays(borrowDate, LOAN_PERIOD_DAYS);
    if (!isValidDate(dueDate)) {
        return circulationError(
            "The calculated due date is outside the supported date range.");
    }
    if (isFutureDate(borrowDate)) {
        return circulationError("Borrowing date cannot be in the future.");
    }
    std::string paymentError;
    const double totalDue = BORROWING_FEE + REFUNDABLE_DEPOSIT;
    if (!validCheckoutPayment(paymentMethod, cashReceived, totalDue,
                              paymentError)) {
        return circulationError(paymentError);
    }
    Member* member = findMember(data, memberId);
    Book* book = findBook(data, bookId);
    if (member == nullptr || !member->active) {
        return circulationError("Active member not found.");
    }
    if (book == nullptr || !book->active) {
        return circulationError("Active book not found.");
    }
    if (member->outstandingFine > 0.005) {
        return circulationError("Please pay the outstanding fine before borrowing.");
    }
    if (countActiveLoans(data, memberId) >= MAX_ACTIVE_LOANS) {
        return circulationError("The maximum of five active loans has been reached.");
    }
    for (const Loan& loan : data.loans) {
        if (sameText(loan.memberId, memberId) && sameText(loan.bookId, bookId) &&
            loan.status == LoanStatus::Active) {
            return circulationError("This member already has an active loan for the book.");
        }
    }

    Reservation* reservation = findActiveReservation(data, memberId, bookId);
    const bool hasReadyReservation =
        reservation != nullptr && reservation->status == ReservationStatus::Ready;
    if (reservation != nullptr && reservation->status == ReservationStatus::Waiting) {
        return circulationError(
            "The reservation is still waiting. Another member is ahead in the queue.");
    }

    // A READY reservation holds a real copy. A physical copy must still exist
    // before it can be borrowed, which prevents negative availability.
    if (book->availableCopies <= 0) {
        return circulationError("No physical copy is currently available.");
    }
    if (!hasReadyReservation &&
        copiesAvailableForGeneralBorrowing(data, bookId) <= 0) {
        return circulationError(
            "No unreserved copy is available. The member may place a reservation.");
    }

    const LibraryData original = data;
    if (hasReadyReservation) {
        reservation->status = ReservationStatus::Fulfilled;
    }

    // One borrowing updates the loan, inventory and payment vectors together.
    Loan loan;
    loan.id = data.nextLoanId++;
    loan.memberId = member->id;
    loan.bookId = book->id;
    loan.borrowDate = borrowDate;
    loan.dueDate = dueDate;
    loan.status = LoanStatus::Active;
    loan.borrowingFee = BORROWING_FEE;
    loan.depositPaid = REFUNDABLE_DEPOSIT;
    data.loans.push_back(loan);
    // The physical copy leaves the library and the history total increases.
    --book->availableCopies;
    ++book->borrowCount;

    addPaymentRecord(data, member->id, "BORROWING_FEE", BORROWING_FEE,
                     borrowDate, loan.id,
                     paymentNote("Non-refundable borrowing fee", paymentMethod,
                                 cashReceived, totalDue),
                     paymentMethod);
    addPaymentRecord(data, member->id, "DEPOSIT", REFUNDABLE_DEPOSIT,
                     borrowDate, loan.id,
                     "Refundable book deposit received; paid by " +
                         paymentMethodLabel(paymentMethod),
                     paymentMethod);

    std::string saveError;
    // Restore every shared vector if saving any part of the borrowing fails.
    if (!saveAllData(data, saveError)) {
        data = original;
        return circulationError("Borrowing was not recorded: " + saveError);
    }

    std::ostringstream message;
    message << "Borrowing confirmed. Loan #" << loan.id << ", due "
            << formatDate(loan.dueDate) << ". Fee RM" << std::fixed
            << std::setprecision(2) << BORROWING_FEE << " and deposit RM"
            << REFUNDABLE_DEPOSIT << " received.";
    return OperationResult{true, message.str(), loan.id,
                           BORROWING_FEE + REFUNDABLE_DEPOSIT,
                           REFUNDABLE_DEPOSIT};
}

// Contributions: Tan Yi Heng handles returns and inventory updates;
// Cheah Yong Shun handles overdue fines and deposit-refund calculations.
OperationResult returnBook(LibraryData& data, const std::string& memberId,
                           int loanId, const Date& returnDate,
                           bool goodCondition) {
    // The loan must be active, belong to this member and use a valid return date.
    if (!isValidDate(returnDate)) {
        return circulationError("Return date is invalid.");
    }
    if (isFutureDate(returnDate)) {
        return circulationError("Return date cannot be in the future.");
    }
    Loan* loan = findLoan(data, loanId);
    if (loan == nullptr || loan->status != LoanStatus::Active ||
        !sameText(loan->memberId, memberId)) {
        return circulationError("Active loan not found for this member.");
    }
    if (daysBetween(loan->borrowDate, returnDate) < 0) {
        return circulationError("Return date cannot be earlier than the borrowing date.");
    }
    Member* member = findMember(data, memberId);
    Book* book = findBook(data, loan->bookId);
    if (member == nullptr || book == nullptr) {
        return circulationError("Related member or book record is missing.");
    }

    const LibraryData original = data;
    // Only days after the due date are charged; an early return has zero days.
    int overdueDays = daysBetween(loan->dueDate, returnDate);
    if (overdueDays < 0) {
        overdueDays = 0;
    }
    const double fine = overdueDays * OVERDUE_FINE_PER_DAY;
    // The deposit is refunded only for an on-time book in good condition.
    const double refund = goodCondition && overdueDays == 0
                              ? loan->depositPaid
                              : 0.0;

    loan->status = LoanStatus::Returned;
    loan->returnDate = returnDate;
    loan->fineCharged = fine;
    loan->depositRefunded = refund;
    loan->returnedInGoodCondition = goodCondition;
    member->outstandingFine += fine;

    // A good copy returns to available stock; a damaged copy leaves inventory.
    if (goodCondition) {
        ++book->availableCopies;
    } else {
        --book->totalCopies;
        if (book->totalCopies < 0) {
            book->totalCopies = 0;
        }
    }

    // Charges and refunds are stored as payment records for later reporting.
    if (fine > 0.005) {
        addPaymentRecord(data, member->id, "FINE_CHARGE", fine, returnDate,
                         loan->id,
                         std::to_string(overdueDays) + " overdue day(s)");
    }
    if (refund > 0.005) {
        addPaymentRecord(data, member->id, "DEPOSIT_REFUND", -refund,
                         returnDate, loan->id, "Deposit refunded in full");
    }
    processReservationQueue(data, book->id, returnDate);

    std::string saveError;
    // Restore the original shared data if the completed return cannot be saved.
    if (!saveAllData(data, saveError)) {
        data = original;
        return circulationError("Return was not recorded: " + saveError);
    }

    std::ostringstream message;
    message << "Return recorded. Overdue fine: RM" << std::fixed
            << std::setprecision(2) << fine << ". Deposit refund: RM" << refund;
    if (!goodCondition) {
        message << ". Damaged copy removed from inventory.";
    } else if (refund < 0.005) {
        message << ". Deposit withheld because the return was late.";
    }
    return OperationResult{true, message.str(), loanId, fine, refund};
}

// Contribution: Cheah Yong Shun (2505229)
// Validates a fine payment and updates the balance and payment history.
OperationResult payFine(LibraryData& data, const std::string& memberId,
                        double amount, const Date& paymentDate,
                        const std::string& paymentMethod,
                        double cashReceived) {
    if (!isValidDate(paymentDate)) {
        return circulationError("Payment date is invalid.");
    }
    if (isFutureDate(paymentDate)) {
        return circulationError("Payment date cannot be in the future.");
    }
    Member* member = findMember(data, memberId);
    if (member == nullptr || !member->active) {
        return circulationError("Active member not found.");
    }
    if (member->outstandingFine < 0.005) {
        return circulationError("There is no outstanding fine to pay.");
    }
    if (!std::isfinite(amount) || amount <= 0.0) {
        return circulationError("Payment amount must be greater than zero.");
    }
    std::string paymentError;
    if (!validCheckoutPayment(paymentMethod, cashReceived, amount,
                              paymentError)) {
        return circulationError(paymentError);
    }
    if (amount - member->outstandingFine > 0.005) {
        return circulationError("Payment cannot exceed the outstanding balance.");
    }

    // A fine cannot be paid before the latest charge that created the balance.
    bool fineChargeFound = false;
    Date latestFineChargeDate;
    for (const Payment& payment : data.payments) {
        if (sameText(payment.memberId, memberId) &&
            payment.type == "FINE_CHARGE" &&
            (!fineChargeFound ||
             dateToSerial(payment.date) > dateToSerial(latestFineChargeDate))) {
            latestFineChargeDate = payment.date;
            fineChargeFound = true;
        }
    }
    if (fineChargeFound &&
        dateToSerial(paymentDate) < dateToSerial(latestFineChargeDate)) {
        return circulationError(
            "Payment date cannot be earlier than the fine charge.");
    }

    const LibraryData original = data;
    // Subtract the accepted payment and clear very small rounding balances.
    member->outstandingFine -= amount;
    if (member->outstandingFine < 0.005) {
        member->outstandingFine = 0.0;
    }
    addPaymentRecord(data, member->id, "FINE_PAYMENT", amount, paymentDate, 0,
                     paymentNote("Fine payment received", paymentMethod,
                                 cashReceived, amount),
                     paymentMethod);

    std::string saveError;
    // Restore the old balance and payment list if saving fails.
    if (!saveAllData(data, saveError)) {
        data = original;
        return circulationError("Payment was not recorded: " + saveError);
    }

    std::ostringstream message;
    message << "Payment successful. Remaining fine: RM" << std::fixed
            << std::setprecision(2) << member->outstandingFine << '.';
    return OperationResult{true, message.str(), data.nextPaymentId - 1, amount,
                           member->outstandingFine};
}

}  // namespace lms
