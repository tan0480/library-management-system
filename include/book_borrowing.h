// Modules 2 and 3: Book Borrowing, Returns, Fines and Payments
// Contributors:
// - Tan Yi Heng (2507419): borrowing, returns and inventory updates
// - Cheah Yong Shun (2505229): overdue fines and fine-payment integration

#ifndef BOOK_BORROWING_H
#define BOOK_BORROWING_H

#include "library_models.h"

#include <string>

namespace lms {

// Counts active loans for one member and returns the total.
int countActiveLoans(const LibraryData& data, const std::string& memberId);
// Creates a loan, decreases available stock and records fee/deposit payments.
OperationResult borrowBook(LibraryData& data, const std::string& memberId,
                           const std::string& bookId, const Date& borrowDate,
                           const std::string& paymentMethod,
                           double cashReceived);
// Completes a loan, calculates fine/refund and advances the reservation queue.
OperationResult returnBook(LibraryData& data, const std::string& memberId,
                           int loanId, const Date& returnDate,
                           bool goodCondition);
// Validates a fine payment and reduces the member's outstanding balance.
OperationResult payFine(LibraryData& data, const std::string& memberId,
                        double amount, const Date& paymentDate,
                        const std::string& paymentMethod,
                        double cashReceived);

}  // namespace lms

#endif
