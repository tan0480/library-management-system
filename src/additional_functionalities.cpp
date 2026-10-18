// Joint Additional Functionalities
// Contributors: All four group members
// Purpose: Manages reservations, reservation queues, held copies,
// borrowing history and top-borrowed recommendations.

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

OperationResult additionalFeatureError(const std::string& message) {
    return OperationResult{false, message};
}

bool sameText(const std::string& left, const std::string& right) {
    return lowerCopy(left) == lowerCopy(right);
}

bool validReservationPayment(const std::string& method, double cashReceived) {
    return isCustomerPaymentMethod(method) &&
           (method != "CASH" ||
            (std::isfinite(cashReceived) &&
             cashReceived + 0.005 >= RESERVATION_FEE));
}

std::string reservationPaymentNote(const std::string& method,
                                   double cashReceived) {
    std::ostringstream note;
    note << "Non-refundable reservation fee; paid by "
         << paymentMethodLabel(method);
    if (method == "CASH") {
        note << "; cash received RM" << std::fixed << std::setprecision(2)
             << cashReceived << "; change RM"
             << cashReceived - RESERVATION_FEE;
    }
    return note.str();
}

Date estimatedAvailability(const LibraryData& data, const std::string& bookId,
                           const Date& reservationDate) {
    // The earliest active due date gives the member a useful pickup estimate.
    bool activeLoanFound = false;
    Date earliestDueDate;
    for (const Loan& loan : data.loans) {
        if (sameText(loan.bookId, bookId) && loan.status == LoanStatus::Active) {
            if (!activeLoanFound ||
                dateToSerial(loan.dueDate) < dateToSerial(earliestDueDate)) {
                earliestDueDate = loan.dueDate;
                activeLoanFound = true;
            }
        }
    }
    if (!activeLoanFound) {
        return addDays(reservationDate, LOAN_PERIOD_DAYS);
    }
    // An estimate must never be earlier than the date the reservation was made.
    if (dateToSerial(earliestDueDate) < dateToSerial(reservationDate)) {
        return reservationDate;
    }
    return earliestDueDate;
}

}  // namespace

Reservation* findActiveReservation(LibraryData& data,
                                   const std::string& memberId,
                                   const std::string& bookId) {
    for (Reservation& reservation : data.reservations) {
        if (sameText(reservation.memberId, memberId) &&
            sameText(reservation.bookId, bookId) &&
            (reservation.status == ReservationStatus::Waiting ||
             reservation.status == ReservationStatus::Ready)) {
            return &reservation;
        }
    }
    return nullptr;
}

int heldCopiesForReservations(const LibraryData& data, const std::string& bookId) {
    // Each READY reservation represents one physical copy waiting for pickup.
    int heldCopies = 0;
    for (const Reservation& reservation : data.reservations) {
        if (sameText(reservation.bookId, bookId) &&
            reservation.status == ReservationStatus::Ready) {
            ++heldCopies;
        }
    }
    return heldCopies;
}

int copiesAvailableForGeneralBorrowing(const LibraryData& data,
                                       const std::string& bookId) {
    const Book* book = findBook(data, bookId);
    if (book == nullptr || !book->active) {
        return 0;
    }
    // READY holds are subtracted before offering copies to other members.
    const int available =
        book->availableCopies - heldCopiesForReservations(data, bookId);
    if (available < 0) {
        return 0;
    }
    return available;
}

OperationResult reserveBook(LibraryData& data, const std::string& memberId,
                            const std::string& bookId,
                            const Date& reservationDate,
                            const std::string& paymentMethod,
                            double cashReceived) {
    // Reservations are allowed only when no unreserved copy can be borrowed.
    if (!isValidDate(reservationDate)) {
        return additionalFeatureError("Reservation date is invalid.");
    }
    if (isFutureDate(reservationDate)) {
        return additionalFeatureError("Reservation date cannot be in the future.");
    }
    if (!validReservationPayment(paymentMethod, cashReceived)) {
        return additionalFeatureError(
            "Select a valid payment method and provide enough cash.");
    }
    Member* member = findMember(data, memberId);
    const Book* book = findBook(data, bookId);
    if (member == nullptr || !member->active) {
        return additionalFeatureError("Active member not found.");
    }
    if (book == nullptr || !book->active) {
        return additionalFeatureError("Active book not found.");
    }
    if (copiesAvailableForGeneralBorrowing(data, bookId) > 0) {
        return additionalFeatureError("A copy is available now; borrow it instead.");
    }
    if (findActiveReservation(data, memberId, bookId) != nullptr) {
        return additionalFeatureError("This member already has an active reservation.");
    }
    for (const Loan& loan : data.loans) {
        if (sameText(loan.memberId, memberId) && sameText(loan.bookId, bookId) &&
            loan.status == LoanStatus::Active) {
            return additionalFeatureError(
                "A member cannot reserve a book already borrowed.");
        }
    }

    const LibraryData original = data;
    // New reservations start as WAITING and their IDs preserve FIFO order.
    Reservation reservation;
    reservation.id = data.nextReservationId++;
    reservation.memberId = member->id;
    reservation.bookId = book->id;
    reservation.reservationDate = reservationDate;
    reservation.estimatedAvailableDate =
        estimatedAvailability(data, bookId, reservationDate);
    reservation.status = ReservationStatus::Waiting;
    reservation.reservationFee = RESERVATION_FEE;
    data.reservations.push_back(reservation);
    addPaymentRecord(data, member->id, "RESERVATION_FEE", RESERVATION_FEE,
                     reservationDate, reservation.id,
                     reservationPaymentNote(paymentMethod, cashReceived),
                     paymentMethod);

    std::string saveError;
    // Restore the queue and payment list if the reservation cannot be saved.
    if (!saveAllData(data, saveError)) {
        data = original;
        return additionalFeatureError("Reservation was not recorded: " + saveError);
    }

    std::ostringstream message;
    message << "Reservation #" << reservation.id
            << " added to the FIFO queue. Estimated availability: "
            << formatDate(reservation.estimatedAvailableDate)
            << ". Fee RM" << std::fixed << std::setprecision(2)
            << RESERVATION_FEE << " received.";
    return OperationResult{true, message.str(), reservation.id, RESERVATION_FEE};
}

OperationResult cancelReservation(LibraryData& data,
                                  const std::string& memberId,
                                  int reservationId) {
    Reservation* match = nullptr;
    for (Reservation& reservation : data.reservations) {
        if (reservation.id == reservationId &&
            sameText(reservation.memberId, memberId) &&
            (reservation.status == ReservationStatus::Waiting ||
             reservation.status == ReservationStatus::Ready)) {
            match = &reservation;
            break;
        }
    }
    if (match == nullptr) {
        return additionalFeatureError("Active reservation not found for this member.");
    }

    const LibraryData original = data;
    const std::string bookId = match->bookId;
    // Cancelling a READY reservation releases its held copy to the next member.
    const bool releasedHeldCopy = match->status == ReservationStatus::Ready;
    match->status = ReservationStatus::Cancelled;
    match->notified = false;
    if (releasedHeldCopy) {
        processReservationQueue(data, bookId, todayLocal());
    }

    std::string saveError;
    if (!saveAllData(data, saveError)) {
        data = original;
        return additionalFeatureError("Reservation was not cancelled: " + saveError);
    }
    return OperationResult{
        true, "Reservation cancelled. The reservation fee is non-refundable."};
}

void processReservationQueue(LibraryData& data, const std::string& bookId,
                             const Date& availableDate) {
    // Assign one uncommitted physical copy at a time. Selecting the smallest
    // reservation ID preserves the order in which reservations were created.
    while (copiesAvailableForGeneralBorrowing(data, bookId) > 0) {
        Reservation* next = nullptr;
        for (Reservation& reservation : data.reservations) {
            if (sameText(reservation.bookId, bookId) &&
                reservation.status == ReservationStatus::Waiting &&
                (next == nullptr || reservation.id < next->id)) {
                next = &reservation;
            }
        }
        if (next == nullptr) {
            break;
        }
        next->status = ReservationStatus::Ready;
        next->notified = false;
        next->estimatedAvailableDate = availableDate;
    }
}

std::vector<const Book*> topBorrowedBooks(const LibraryData& data,
                                          std::size_t limit) {
    // Only books with real loan records can appear in the recommendations.
    std::vector<const Book*> ranked;
    for (const Book& book : data.books) {
        if (book.active && borrowingHistoryCount(data, book.id) > 0) {
            ranked.push_back(&book);
        }
    }
    // A selection sort ranks books by actual borrowing-history records.
    for (std::size_t position = 0; position < ranked.size(); ++position) {
        std::size_t bestPosition = position;
        for (std::size_t candidate = position + 1;
             candidate < ranked.size(); ++candidate) {
            const std::size_t candidateCount =
                borrowingHistoryCount(data, ranked[candidate]->id);
            const std::size_t bestCount =
                borrowingHistoryCount(data, ranked[bestPosition]->id);
            if (candidateCount > bestCount ||
                (candidateCount == bestCount &&
                 lowerCopy(ranked[candidate]->title) <
                     lowerCopy(ranked[bestPosition]->title))) {
                bestPosition = candidate;
            }
        }
        if (bestPosition != position) {
            const Book* temporary = ranked[position];
            ranked[position] = ranked[bestPosition];
            ranked[bestPosition] = temporary;
        }
    }
    if (ranked.size() > limit) {
        ranked.resize(limit);
    }
    return ranked;
}

std::size_t borrowingHistoryCount(const LibraryData& data,
                                  const std::string& bookId) {
    std::size_t count = 0;
    for (const Loan& loan : data.loans) {
        if (lowerCopy(loan.bookId) == lowerCopy(bookId)) {
            ++count;
        }
    }
    return count;
}

std::vector<const Loan*> borrowingHistoryForBook(const LibraryData& data,
                                                 const std::string& bookId) {
    std::vector<const Loan*> history;
    for (const Loan& loan : data.loans) {
        if (lowerCopy(loan.bookId) == lowerCopy(bookId)) {
            history.push_back(&loan);
        }
    }
    // A second simple selection sort puts the newest loan records first.
    for (std::size_t position = 0; position < history.size(); ++position) {
        std::size_t newestPosition = position;
        for (std::size_t candidate = position + 1;
             candidate < history.size(); ++candidate) {
            if (dateToSerial(history[candidate]->borrowDate) >
                dateToSerial(history[newestPosition]->borrowDate)) {
                newestPosition = candidate;
            }
        }
        if (newestPosition != position) {
            const Loan* temporary = history[position];
            history[position] = history[newestPosition];
            history[newestPosition] = temporary;
        }
    }
    return history;
}

}  // namespace lms
