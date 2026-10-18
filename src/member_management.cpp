// Module 1: Member Management
// Contributor: Lam Yi Hen (2505880)
// Purpose: Manages registration, authentication, member searches,
// profile updates and member-record deactivation.

#include "member_management.h"

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

OperationResult validationError(const std::string& message) {
    return OperationResult{false, message};
}

// Keeps registration and profile updates under the same contact-detail rules.
OperationResult validateMemberDetails(const Member& member) {
    if (!isValidName(member.name)) {
        return validationError(
            "Name must contain only letters, spaces, apostrophes or hyphens.");
    }
    if (!isValidPhone(member.phone)) {
        return validationError(
            "Phone number must use +60 followed by 9 or 10 digits without spaces or dashes.");
    }
    if (!isValidEmail(member.email)) {
        return validationError("Please enter a valid email address.");
    }
    return OperationResult{true, "Member details are valid."};
}

}  // namespace

// Finds the highest existing M-number so deactivated IDs are not reused.
std::string generateNextMemberId(const LibraryData& data) {
    int highestNumber = 0;
    for (const Member& member : data.members) {
        const std::string id = trim(member.id);
        if (id.size() < 2 || (id[0] != 'M' && id[0] != 'm')) {
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
    generatedId << 'M' << std::setfill('0') << std::setw(3)
                << highestNumber + 1;
    return generatedId.str();
}

// Registers a validated member and stores the new record in LibraryData.
OperationResult registerMember(LibraryData& data, const Member& member) {
    Member candidate = member;
    candidate.id = generateNextMemberId(data);
    candidate.name = trim(candidate.name);
    candidate.phone = trim(candidate.phone);
    candidate.email = trim(candidate.email);
    candidate.active = true;
    candidate.outstandingFine = 0.0;

    if (!isValidIdentifier(candidate.id)) {
        return validationError(
            "Member ID must contain 2-15 letters, digits, '-' or '_'.");
    }
    // Member IDs must be unique because they link loans, reservations and payments.
    if (findMember(data, candidate.id) != nullptr) {
        return validationError("The member ID already exists.");
    }
    if (!isValidPassword(candidate.password)) {
        return validationError(
            "Password must contain 6-30 characters without spaces.");
    }
    const OperationResult detailsResult = validateMemberDetails(candidate);
    if (!detailsResult.success) {
        return detailsResult;
    }

    data.members.push_back(candidate);
    std::string saveMessage;
    // Remove the new record from memory if the updated files cannot be saved.
    if (!saveAllData(data, saveMessage)) {
        data.members.pop_back();
        return validationError("Member was not added: " + saveMessage);
    }
    return OperationResult{true, "Member registered successfully."};
}

bool authenticateMember(const LibraryData& data, const std::string& memberId,
                        const std::string& password) {
    const Member* member = findMember(data, trim(memberId));
    return member != nullptr && member->active && member->password == password;
}

bool authenticateLibrarian(const std::string& userId, const std::string& password) {
    return lowerCopy(trim(userId)) == "admin" && password == "admin123";
}

OperationResult updateMemberProfile(LibraryData& data, const std::string& memberId,
                                    const std::string& name,
                                    const std::string& phone,
                                    const std::string& email) {
    Member* member = findMember(data, memberId);
    if (member == nullptr || !member->active) {
        return validationError("Active member not found.");
    }

    const std::string cleanedName = trim(name);
    const std::string cleanedPhone = trim(phone);
    const std::string cleanedEmail = trim(email);
    Member candidate = *member;
    candidate.name = cleanedName;
    candidate.phone = cleanedPhone;
    candidate.email = cleanedEmail;
    const OperationResult detailsResult = validateMemberDetails(candidate);
    if (!detailsResult.success) {
        return detailsResult;
    }

    // Keep the original details so a failed save does not leave memory changed.
    const Member original = *member;
    member->name = cleanedName;
    member->phone = cleanedPhone;
    member->email = cleanedEmail;
    std::string saveMessage;
    if (!saveAllData(data, saveMessage)) {
        *member = original;
        return validationError("Profile was not updated: " + saveMessage);
    }
    return OperationResult{true, "Member profile updated successfully."};
}

OperationResult deleteMember(LibraryData& data, const std::string& memberId) {
    Member* member = findMember(data, memberId);
    if (member == nullptr || !member->active) {
        return validationError("Active member not found.");
    }
    // Protect related records by blocking deletion while obligations remain.
    if (member->outstandingFine > 0.005) {
        return validationError(
            "Member cannot be deleted while an outstanding fine remains.");
    }
    for (const Loan& loan : data.loans) {
        if (lowerCopy(loan.memberId) == lowerCopy(memberId) &&
            loan.status == LoanStatus::Active) {
            return validationError(
                "Member cannot be deleted while a book is still borrowed.");
        }
    }
    for (const Reservation& reservation : data.reservations) {
        if (lowerCopy(reservation.memberId) == lowerCopy(memberId) &&
            (reservation.status == ReservationStatus::Waiting ||
             reservation.status == ReservationStatus::Ready)) {
            return validationError(
                "Member cannot be deleted while a reservation is active.");
        }
    }

    // Records are deactivated instead of erased so past histories stay valid.
    member->active = false;
    std::string saveMessage;
    if (!saveAllData(data, saveMessage)) {
        member->active = true;
        return validationError("Member was not deleted: " + saveMessage);
    }
    return OperationResult{true, "Member record deactivated successfully."};
}

std::vector<const Member*> searchMembers(const LibraryData& data,
                                         const std::string& keyword) {
    const std::string wanted = lowerCopy(trim(keyword));
    std::vector<const Member*> matches;
    for (const Member& member : data.members) {
        if (!member.active) {
            continue;
        }
        if (wanted.empty() || lowerCopy(member.id).find(wanted) != std::string::npos ||
            lowerCopy(member.name).find(wanted) != std::string::npos ||
            lowerCopy(member.phone).find(wanted) != std::string::npos ||
            lowerCopy(member.email).find(wanted) != std::string::npos) {
            matches.push_back(&member);
        }
    }
    return matches;
}

}  // namespace lms
