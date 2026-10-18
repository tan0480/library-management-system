// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Validates member fields, dates, menu choices and payment input.

#ifndef INPUT_VALIDATION_H
#define INPUT_VALIDATION_H

#include "library_models.h"

#include <string>

namespace lms {

// Returns true when a name contains only accepted characters.
bool isValidName(const std::string& name);
// Returns true for +60 followed by 9 or 10 digits, without spaces or dashes.
bool isValidPhone(const std::string& phone);
// Returns true when a password contains 6-30 non-whitespace characters.
bool isValidPassword(const std::string& password);
// Returns true when an email address has a basic valid format.
bool isValidEmail(const std::string& email);
// Returns true when an ID contains only accepted characters.
bool isValidIdentifier(const std::string& id);

// Reads one console line or ends the program safely if the stream closes.
void readConsoleLineOrExit(std::string& value);
// Repeats until the user enters a whole number inside the given range.
int readIntInRange(const std::string& prompt, int minimum, int maximum);
// Repeats until the user enters a positive amount within the limit.
double readPositiveAmount(const std::string& prompt, double maximum);
// Repeats until the user enters non-empty text.
std::string readRequiredLine(const std::string& prompt);
// Repeats until the user enters a valid member or book identifier.
std::string readValidIdentifier(const std::string& prompt);
// Repeats until the user enters a valid personal name.
std::string readValidName(const std::string& prompt);
// Repeats until the user enters a Malaysian +60 phone number.
std::string readValidPhone(const std::string& prompt);
// Repeats until the user enters a basic valid email address.
std::string readValidEmail(const std::string& prompt);
// Repeats until the user enters a valid password.
std::string readValidPassword(const std::string& prompt);
// Repeats until the user enters a valid YYYY-MM-DD date.
Date readDate(const std::string& prompt);
// Repeats until the user enters a valid date that is today or earlier.
Date readDateNotInFuture(const std::string& prompt);
// Repeats until the user enters a yes or no response.
bool readYesNo(const std::string& prompt);

}  // namespace lms

#endif
