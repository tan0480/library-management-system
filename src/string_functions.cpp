// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Provides shared trimming and case-conversion functions.

#include "string_functions.h"

#include <cctype>

using namespace std;

namespace lms {

std::string trim(const std::string& text) {
    // Find the first and last non-space positions without changing inner spaces.
    std::size_t first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }

    std::size_t last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
        --last;
    }
    return text.substr(first, last - first);
}

std::string lowerCopy(const std::string& text) {
    // A lowercase copy supports case-insensitive IDs and keyword searches.
    std::string result = text;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(result[index]);
        result[index] = static_cast<char>(std::tolower(character));
    }
    return result;
}

}  // namespace lms
