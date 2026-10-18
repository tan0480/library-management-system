// Module 3: Fine and Payment Management
// Contributor: Cheah Yong Shun (2505229)
// Purpose: Creates payment records for fees, deposits, refunds and fines.

#ifndef PAYMENT_FUNCTIONS_H
#define PAYMENT_FUNCTIONS_H

#include "library_models.h"

#include <string>

namespace lms {

// Returns true for one of the four payment methods offered to members.
bool isCustomerPaymentMethod(const std::string& method);
// Converts a stored method code into a readable receipt/report label.
std::string paymentMethodLabel(const std::string& method);

// Creates one Payment record and appends it to the payment vector.
void addPaymentRecord(LibraryData& data, const std::string& memberId,
                      const std::string& type, double amount, const Date& date,
                      int referenceId, const std::string& note,
                      const std::string& method = "NOT_APPLICABLE");

}  // namespace lms

#endif
