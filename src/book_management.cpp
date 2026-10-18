// Module 2: Book Management
// Contributor: Tan Yi Heng (2507419)
// Purpose: Manages book searches, catalogue records and inventory quantities.

#include "book_management.h"

#include "additional_functionalities.h"
#include "date_functions.h"
#include "input_validation.h"
#include "record_search.h"
#include "storage.h"
#include "string_functions.h"

#include <cctype>
#include <iomanip>
#include <sstream>

using namespace std;

namespace lms {
namespace {

OperationResult bookError(const std::string& message) {
    return OperationResult{false, message};
}

bool validBookText(const std::string& text, std::size_t maximumLength) {
    const std::string cleaned = trim(text);
    return !cleaned.empty() && cleaned.size() <= maximumLength &&
           cleaned.find('|') == std::string::npos;
}

}  // namespace

// Keeps category names consistent for book creation, editing and validation.
const std::vector<std::string>& bookCategories() {
    static const std::vector<std::string> categories = {
        "Biography", "Business", "Classic", "Dystopian", "Fantasy",
        "Fiction", "Finance", "History", "Mystery", "Science",
        "Self Development", "Technology"};
    return categories;
}

bool isValidBookCategory(const std::string& category) {
    const std::string cleanedCategory = lowerCopy(trim(category));
    const std::vector<std::string>& categories = bookCategories();
    for (std::size_t index = 0; index < categories.size(); ++index) {
        if (cleanedCategory == lowerCopy(categories[index])) {
            return true;
        }
    }
    return false;
}

// Finds the highest existing B-number so deactivated IDs are not reused.
std::string generateNextBookId(const LibraryData& data) {
    int highestNumber = 0;
    for (const Book& book : data.books) {
        const std::string id = trim(book.id);
        if (id.size() < 2 || (id[0] != 'B' && id[0] != 'b')) {
            continue;
        }

        int number = 0;
        bool validNumber = true;
        for (std::size_t index = 1; index < id.size(); ++index) {
            const unsigned char character =
                static_cast<unsigned char>(id[index]);
            if (std::isdigit(character) == 0) {
                validNumber = false;
                break;
            }
            number = number * 10 + (id[index] - '0');
        }
        if (validNumber && number > highestNumber) {
            highestNumber = number;
        }
    }

    std::ostringstream generatedId;
    generatedId << 'B' << std::setfill('0') << std::setw(3)
                << highestNumber + 1;
    return generatedId.str();
}

// Adds a validated catalogue record and sets all copies as available.
OperationResult addBook(LibraryData& data, const Book& book) {
    Book candidate = book;
    candidate.id = generateNextBookId(data);
    candidate.title = trim(candidate.title);
    candidate.author = trim(candidate.author);
    candidate.category = trim(candidate.category);
    candidate.availableCopies = candidate.totalCopies;
    candidate.borrowCount = 0;
    candidate.active = true;

    if (!isValidIdentifier(candidate.id)) {
        return bookError("Book ID must contain 2-15 letters, digits, '-' or '_'.");
    }
    // A duplicate Book ID would make loans and reservations point to two books.
    if (findBook(data, candidate.id) != nullptr) {
        return bookError("The book ID already exists.");
    }
    if (!validBookText(candidate.title, 100)) {
        return bookError("Book title is required and must not exceed 100 characters.");
    }
    if (!validBookText(candidate.author, 60)) {
        return bookError("Author is required and must not exceed 60 characters.");
    }
    if (!isValidBookCategory(candidate.category)) {
        return bookError("Please select a valid predefined book category.");
    }
    if (candidate.totalCopies < 1 || candidate.totalCopies > 999) {
        return bookError("Total copies must be between 1 and 999.");
    }

    data.books.push_back(candidate);
    std::string saveMessage;
    // Remove the new book from memory if the files cannot be saved.
    if (!saveAllData(data, saveMessage)) {
        data.books.pop_back();
        return bookError("Book was not added: " + saveMessage);
    }
    return OperationResult{true, "Book added to the catalogue successfully."};
}

OperationResult updateBook(LibraryData& data, const std::string& bookId,
                           const std::string& title,
                           const std::string& author,
                           const std::string& category,
                           int newTotalCopies) {
    Book* book = findBook(data, bookId);
    if (book == nullptr || !book->active) {
        return bookError("Active book not found.");
    }

    const std::string cleanedTitle = trim(title);
    const std::string cleanedAuthor = trim(author);
    const std::string cleanedCategory = trim(category);
    if (!validBookText(cleanedTitle, 100) ||
        !validBookText(cleanedAuthor, 60)) {
        return bookError("Title or author is invalid.");
    }
    if (!isValidBookCategory(cleanedCategory)) {
        return bookError("Please select a valid predefined book category.");
    }

    // READY reservations hold physical copies that are still counted as
    // available. Stock may not be reduced below loans plus those held copies.
    const int borrowedCopies = book->totalCopies - book->availableCopies;
    const int heldCopies = heldCopiesForReservations(data, bookId);
    const int minimumRequiredCopies = borrowedCopies + heldCopies;
    if (newTotalCopies < minimumRequiredCopies || newTotalCopies < 1 ||
        newTotalCopies > 999) {
        return bookError(
            "Total copies cannot be less than copies on loan plus copies held "
            "for ready reservations.");
    }

    // The complete data snapshot also protects reservation state if saving
    // fails after an inventory increase advances the FIFO queue.
    const LibraryData original = data;
    const int previousTotalCopies = book->totalCopies;
    book->title = cleanedTitle;
    book->author = cleanedAuthor;
    book->category = cleanedCategory;
    book->totalCopies = newTotalCopies;
    book->availableCopies = newTotalCopies - borrowedCopies;

    // Newly added copies must be offered to waiting reservations before they
    // become available to general borrowers.
    if (newTotalCopies > previousTotalCopies) {
        processReservationQueue(data, bookId, todayLocal());
    }

    std::string saveMessage;
    if (!saveAllData(data, saveMessage)) {
        data = original;
        return bookError("Book was not updated: " + saveMessage);
    }
    return OperationResult{true, "Book information updated successfully."};
}

OperationResult deleteBook(LibraryData& data, const std::string& bookId) {
    Book* book = findBook(data, bookId);
    if (book == nullptr || !book->active) {
        return bookError("Active book not found.");
    }
    for (const Loan& loan : data.loans) {
        if (lowerCopy(loan.bookId) == lowerCopy(bookId) &&
            loan.status == LoanStatus::Active) {
            return bookError("Book cannot be deleted while a copy is on loan.");
        }
    }
    for (const Reservation& reservation : data.reservations) {
        if (lowerCopy(reservation.bookId) == lowerCopy(bookId) &&
            (reservation.status == ReservationStatus::Waiting ||
             reservation.status == ReservationStatus::Ready)) {
            return bookError("Book cannot be deleted while reservations are active.");
        }
    }

    // Deactivation keeps old borrowing and payment histories meaningful.
    book->active = false;
    std::string saveMessage;
    if (!saveAllData(data, saveMessage)) {
        book->active = true;
        return bookError("Book was not deleted: " + saveMessage);
    }
    return OperationResult{true, "Book record deactivated successfully."};
}

std::vector<const Book*> searchBooks(const LibraryData& data,
                                     const std::string& keyword,
                                     bool includeInactive) {
    const std::string wanted = lowerCopy(trim(keyword));
    std::vector<const Book*> matches;
    // One keyword is compared with the main searchable catalogue fields.
    for (const Book& book : data.books) {
        if (!includeInactive && !book.active) {
            continue;
        }
        if (wanted.empty() || lowerCopy(book.id).find(wanted) != std::string::npos ||
            lowerCopy(book.title).find(wanted) != std::string::npos ||
            lowerCopy(book.author).find(wanted) != std::string::npos ||
            lowerCopy(book.category).find(wanted) != std::string::npos) {
            matches.push_back(&book);
        }
    }
    return matches;
}

}  // namespace lms
