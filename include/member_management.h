// Module 1: Member Management
// Contributor: Lam Yi Hen (2505880)
// Purpose: Manages registration, authentication, member searches,
// profile updates and member-record deactivation.

#ifndef MEMBER_MANAGEMENT_H
#define MEMBER_MANAGEMENT_H

#include "library_models.h"

#include <string>
#include <vector>

namespace lms {

// Returns the next unused system member ID, such as M003 or M004.
std::string generateNextMemberId(const LibraryData& data);
// Validates and saves a new member; returns success and an explanatory message.
OperationResult registerMember(LibraryData& data, const Member& member);
// Returns true when an active member ID and password match.
bool authenticateMember(const LibraryData& data, const std::string& memberId,
                        const std::string& password);
// Checks the single librarian account used by this student system.
bool authenticateLibrarian(const std::string& userId, const std::string& password);
// Validates and updates an existing member's contact details.
OperationResult updateMemberProfile(LibraryData& data, const std::string& memberId,
                                    const std::string& name,
                                    const std::string& phone,
                                    const std::string& email);
// Deactivates a member only when no loan, reservation or fine blocks deletion.
OperationResult deleteMember(LibraryData& data, const std::string& memberId);
// Returns active members whose details contain the search keyword.
std::vector<const Member*> searchMembers(const LibraryData& data,
                                         const std::string& keyword);

}  // namespace lms

#endif
