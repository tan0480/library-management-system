// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Provides shared trimming and case-conversion functions.

#ifndef STRING_FUNCTIONS_H
#define STRING_FUNCTIONS_H

#include <string>

namespace lms {

// Returns a copy without whitespace at the beginning or end.
std::string trim(const std::string& text);
// Returns a lowercase copy for case-insensitive comparisons.
std::string lowerCopy(const std::string& text);

}  // namespace lms

#endif
