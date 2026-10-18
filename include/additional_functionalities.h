// Joint Additional Functionalities
// Contributors: All four group members
// Purpose: Manages reservations, reservation queues, held copies,
// borrowing history and top-borrowed recommendations.

#ifndef ADDITIONAL_FUNCTIONALITIES_H
#define ADDITIONAL_FUNCTIONALITIES_H

#include "library_models.h"

#include <cstddef>
#include <string>
#include <vector>

namespace lms {

// Returns a member's WAITING or READY reservation for one book, if present.
Reservation* findActiveReservation(LibraryData& data,
                                   const std::string& memberId,
                                   const std::string& bookId);
// Counts physical copies currently held for READY reservations.
int heldCopiesForReservations(const LibraryData& data, const std::string& bookId);
// Returns available copies after subtracting READY reservation holds.
int copiesAvailableForGeneralBorrowing(const LibraryData& data,
                                       const std::string& bookId);
// Adds an unavailable book to the member's FIFO reservation queue.
OperationResult reserveBook(LibraryData& data, const std::string& memberId,
                            const std::string& bookId,
                            const Date& reservationDate,
                            const std::string& paymentMethod,
                            double cashReceived);
// Cancels one active reservation and offers a released copy to the next member.
OperationResult cancelReservation(LibraryData& data,
                                  const std::string& memberId,
                                  int reservationId);
// Changes WAITING reservations to READY in first-in-first-out order.
void processReservationQueue(LibraryData& data, const std::string& bookId,
                             const Date& availableDate);
// Returns up to limit books ranked by their actual borrowing-history totals.
std::vector<const Book*> topBorrowedBooks(const LibraryData& data,
                                          std::size_t limit);
// Returns the number of recorded loans for one book.
std::size_t borrowingHistoryCount(const LibraryData& data,
                                  const std::string& bookId);
// Returns one book's loan records with the newest record first.
std::vector<const Loan*> borrowingHistoryForBook(const LibraryData& data,
                                                 const std::string& bookId);

}  // namespace lms

#endif
