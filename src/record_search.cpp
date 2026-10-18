// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Searches shared vectors for members, books and loan records.

#include "record_search.h"

#include "string_functions.h"

using namespace std;

namespace lms {

// Record searches ignore letter case so IDs work consistently across modules.
Member* findMember(LibraryData& data, const std::string& memberId) {
    for (Member& member : data.members) {
        if (lowerCopy(member.id) == lowerCopy(memberId)) {
            return &member;
        }
    }
    return nullptr;
}

const Member* findMember(const LibraryData& data, const std::string& memberId) {
    for (const Member& member : data.members) {
        if (lowerCopy(member.id) == lowerCopy(memberId)) {
            return &member;
        }
    }
    return nullptr;
}

Book* findBook(LibraryData& data, const std::string& bookId) {
    for (Book& book : data.books) {
        if (lowerCopy(book.id) == lowerCopy(bookId)) {
            return &book;
        }
    }
    return nullptr;
}

const Book* findBook(const LibraryData& data, const std::string& bookId) {
    for (const Book& book : data.books) {
        if (lowerCopy(book.id) == lowerCopy(bookId)) {
            return &book;
        }
    }
    return nullptr;
}

Loan* findLoan(LibraryData& data, int loanId) {
    for (Loan& loan : data.loans) {
        if (loan.id == loanId) {
            return &loan;
        }
    }
    return nullptr;
}

std::string bookTitle(const LibraryData& data, const std::string& bookId) {
    const Book* book = findBook(data, bookId);
    return book == nullptr ? "Unknown book" : book->title;
}

std::string memberName(const LibraryData& data, const std::string& memberId) {
    const Member* member = findMember(data, memberId);
    return member == nullptr ? "Unknown member" : member->name;
}

}  // namespace lms
