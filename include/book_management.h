// Module 2: Book Management
// Contributor: Tan Yi Heng (2507419)
// Purpose: Manages book searches, catalogue records and inventory quantities.

#ifndef BOOK_MANAGEMENT_H
#define BOOK_MANAGEMENT_H

#include "library_models.h"

#include <string>
#include <vector>

namespace lms {

// Returns the single predefined category list used by catalogue operations.
const std::vector<std::string>& bookCategories();
// Returns true only when a category matches the predefined list.
bool isValidBookCategory(const std::string& category);
// Returns the next unused system Book ID, such as B151 or B152.
std::string generateNextBookId(const LibraryData& data);
// Validates and adds a book, returning the operation result to the caller.
OperationResult addBook(LibraryData& data, const Book& book);
// Updates book details and stock while protecting loans and READY reservations.
OperationResult updateBook(LibraryData& data, const std::string& bookId,
                           const std::string& title,
                           const std::string& author,
                           const std::string& category,
                           int newTotalCopies);
// Deactivates a book only when it has no active loan or reservation.
OperationResult deleteBook(LibraryData& data, const std::string& bookId);
// Returns books matching an ID, title, author or category keyword.
std::vector<const Book*> searchBooks(const LibraryData& data,
                                     const std::string& keyword,
                                     bool includeInactive = false);

}  // namespace lms

#endif
