// Module 3: Fine and Payment Management
// Contributor: Cheah Yong Shun (2505229)
// Purpose: Creates payment records for fees, deposits, refunds and fines.

#include "payment_functions.h"

using namespace std;

namespace lms {

bool isCustomerPaymentMethod(const std::string& method) {
    return method == "CASH" || method == "E_WALLET" ||
           method == "ONLINE_BANKING" || method == "CARD";
}

std::string paymentMethodLabel(const std::string& method) {
    if (method == "E_WALLET") {
        return "E-wallet";
    }
    if (method == "ONLINE_BANKING") {
        return "Online Banking";
    }
    if (method == "CARD") {
        return "Card";
    }
    if (method == "CASH") {
        return "Cash";
    }
    if (method == "NOT_RECORDED") {
        return "Not recorded";
    }
    return "Not applicable";
}

// Creates a shared transaction record used by borrowing, returns and fine payments.
void addPaymentRecord(LibraryData& data, const std::string& memberId,
                      const std::string& type, double amount, const Date& date,
                      int referenceId, const std::string& note,
                      const std::string& method) {
    Payment payment;
    payment.id = data.nextPaymentId++;
    payment.memberId = memberId;
    payment.type = type;
    payment.method = method;
    payment.amount = amount;
    payment.date = date;
    payment.referenceId = referenceId;
    payment.note = note;
    data.payments.push_back(payment);
}

}  // namespace lms
