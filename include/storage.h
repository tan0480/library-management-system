// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Loads and saves the shared library records in text files.

#ifndef STORAGE_H
#define STORAGE_H

#include "library_models.h"

#include <string>

namespace lms {

// Loads existing data or creates demonstration records in the chosen folder.
bool initializeLibrary(LibraryData& data, const std::string& dataDirectory,
                       std::string& message);
// Loads member accounts from Member.txt into the shared data.
bool loadMembers(LibraryData& data, std::string& message);
// Loads catalogue and inventory records from Book.txt.
bool loadBooks(LibraryData& data, std::string& message);
// Loads borrowing and return records from BorrowingHistory.txt.
bool loadLoans(LibraryData& data, std::string& message);
// Loads reservation records from Reservation.txt.
bool loadReservations(LibraryData& data, std::string& message);
// Loads payment records from Payment.txt.
bool loadPayments(LibraryData& data, std::string& message);
// Calls all five load functions and calculates the next record IDs.
bool loadAllData(LibraryData& data, std::string& message);
// Saves all member accounts to Member.txt.
bool saveMembers(const LibraryData& data, std::string& message);
// Saves all catalogue and inventory records to Book.txt.
bool saveBooks(const LibraryData& data, std::string& message);
// Saves all borrowing and return records to BorrowingHistory.txt.
bool saveLoans(const LibraryData& data, std::string& message);
// Saves all reservation records to Reservation.txt.
bool saveReservations(const LibraryData& data, std::string& message);
// Saves all payment records to Payment.txt.
bool savePayments(const LibraryData& data, std::string& message);
// Saves all five files with simple backup-and-restore protection.
bool saveAllData(const LibraryData& data, std::string& message);
// Fills empty storage with the 150-book demonstration dataset.
void seedSampleData(LibraryData& data);

}  // namespace lms

#endif
