// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Searches shared vectors for members, books and loan records.

#ifndef RECORD_SEARCH_H
#define RECORD_SEARCH_H

#include "library_models.h"

#include <string>

namespace lms {

// Finds an editable member record by ID, or returns nullptr.
Member* findMember(LibraryData& data, const std::string& memberId);
// Finds a read-only member record by ID, or returns nullptr.
const Member* findMember(const LibraryData& data, const std::string& memberId);
// Finds an editable book record by ID, or returns nullptr.
Book* findBook(LibraryData& data, const std::string& bookId);
// Finds a read-only book record by ID, or returns nullptr.
const Book* findBook(const LibraryData& data, const std::string& bookId);
// Finds an editable borrowing record by loan ID, or returns nullptr.
Loan* findLoan(LibraryData& data, int loanId);
// Returns a book title for menus and reports.
std::string bookTitle(const LibraryData& data, const std::string& bookId);
// Returns a member name for menus and reports.
std::string memberName(const LibraryData& data, const std::string& memberId);

}  // namespace lms

#endif
