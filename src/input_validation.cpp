// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Validates member fields, dates, menu choices and payment input.

#include "input_validation.h"

#include "date_functions.h"
#include "string_functions.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

namespace lms {

bool isValidName(const std::string& name) {
    // Trim first so accidental outer spaces do not affect the length check.
    const std::string cleaned = trim(name);
    if (cleaned.size() < 2 || cleaned.size() > 60) {
        return false;
    }
    for (std::size_t index = 0; index < cleaned.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(cleaned[index]);
        if (std::isalpha(character) == 0 && cleaned[index] != ' ' &&
            cleaned[index] != '-' && cleaned[index] != '\'') {
            return false;
        }
    }
    return true;
}

bool isValidPhone(const std::string& phone) {
    // Malaysian numbers are stored in international form, for example
    // +60123456789. The digits after +60 must not contain spaces or dashes.
    if ((phone.size() != 12 && phone.size() != 13) ||
        phone[0] != '+' || phone[1] != '6' || phone[2] != '0') {
        return false;
    }
    for (std::size_t index = 3; index < phone.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(phone[index]);
        if (std::isdigit(character) == 0) {
            return false;
        }
    }
    return true;
}

bool isValidPassword(const std::string& password) {
    if (password.size() < 6 || password.size() > 30) {
        return false;
    }
    for (std::size_t index = 0; index < password.size(); ++index) {
        const unsigned char character =
            static_cast<unsigned char>(password[index]);
        if (std::isspace(character) != 0) {
            return false;
        }
    }
    return true;
}

bool isValidEmail(const std::string& email) {
    // Require one @, a later dot and no spaces for a simple diploma-level check.
    const std::size_t at = email.find('@');
    const std::size_t dot = email.find('.', at == std::string::npos ? 0 : at + 1);
    return at > 0 && at == email.rfind('@') && dot != std::string::npos &&
           dot + 1 < email.size() && email.find(' ') == std::string::npos;
}

bool isValidIdentifier(const std::string& id) {
    if (id.size() < 2 || id.size() > 15) {
        return false;
    }
    for (std::size_t index = 0; index < id.size(); ++index) {
        const unsigned char character = static_cast<unsigned char>(id[index]);
        if (std::isalnum(character) == 0 && id[index] != '-' &&
            id[index] != '_') {
            return false;
        }
    }
    return true;
}

void readConsoleLineOrExit(std::string& value) {
    // Checking EOF prevents validation loops from repeating after input closes.
    if (std::getline(std::cin, value)) {
        return;
    }
    std::cout << "\nInput stream closed. Exiting the system safely.\n";
    std::exit(0);
}

int readIntInRange(const std::string& prompt, int minimum, int maximum) {
    // Reading a whole line rejects extra characters after the menu number.
    while (true) {
        std::cout << prompt;
        std::string line;
        readConsoleLineOrExit(line);
        std::istringstream input(line);
        int value = 0;
        const bool numberWasRead = static_cast<bool>(input >> value);
        input >> std::ws;
        if (numberWasRead && input.eof() && value >= minimum && value <= maximum) {
            return value;
        }
        std::cout << "Invalid input. Enter a whole number from " << minimum
                  << " to " << maximum << ".\n";
    }
}

double readPositiveAmount(const std::string& prompt, double maximum) {
    // The loop rejects non-numbers, zero, negative and excessive payments.
    while (true) {
        std::cout << prompt;
        std::string line;
        readConsoleLineOrExit(line);
        std::istringstream input(line);
        double value = 0.0;
        const bool numberWasRead = static_cast<bool>(input >> value);
        input >> std::ws;
        if (numberWasRead && input.eof() && std::isfinite(value) && value > 0.0 &&
            value <= maximum + 0.005) {
            return value;
        }
        std::cout << "Invalid amount. Enter a value greater than RM0.00 and not "
                     "more than RM"
                  << std::fixed << std::setprecision(2) << maximum << ".\n";
    }
}

std::string readRequiredLine(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string value;
        readConsoleLineOrExit(value);
        value = trim(value);
        if (!value.empty()) {
            return value;
        }
        std::cout << "This field is required. Please try again.\n";
    }
}

std::string readValidIdentifier(const std::string& prompt) {
    while (true) {
        const std::string value = readRequiredLine(prompt);
        if (isValidIdentifier(value)) {
            return value;
        }
        std::cout << "Invalid ID. Use 2-15 letters, digits, '-' or '_'.\n";
    }
}

std::string readValidName(const std::string& prompt) {
    while (true) {
        const std::string value = readRequiredLine(prompt);
        if (isValidName(value)) {
            return value;
        }
        std::cout << "Invalid name. Use 2-60 letters, spaces, apostrophes or hyphens.\n";
    }
}

std::string readValidPhone(const std::string& prompt) {
    while (true) {
        const std::string value = readRequiredLine(prompt);
        if (isValidPhone(value)) {
            return value;
        }
        std::cout << "Invalid Malaysian phone number. Enter +60 followed by 9 or "
                     "10 digits, without spaces or dashes. Example: +60123456789.\n";
    }
}

std::string readValidEmail(const std::string& prompt) {
    while (true) {
        const std::string value = readRequiredLine(prompt);
        if (isValidEmail(value)) {
            return value;
        }
        std::cout << "Invalid email address. Example: name@example.com.\n";
    }
}

std::string readValidPassword(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        std::string value;
        readConsoleLineOrExit(value);
        if (isValidPassword(value)) {
            return value;
        }
        std::cout << "Invalid password. Use 6-30 characters without spaces.\n";
    }
}

Date readDate(const std::string& prompt) {
    while (true) {
        const std::string text = readRequiredLine(prompt + " (YYYY-MM-DD): ");
        Date date;
        if (parseDate(text, date)) {
            return date;
        }
        std::cout << "Invalid calendar date. Example: 2026-08-19.\n";
    }
}

// Stops future transaction dates before the user continues to the next screen.
Date readDateNotInFuture(const std::string& prompt) {
    while (true) {
        const Date date = readDate(prompt);
        if (!isFutureDate(date)) {
            return date;
        }
        std::cout << "Future dates are not allowed. Enter today or an earlier date.\n";
    }
}

bool readYesNo(const std::string& prompt) {
    // A do-while loop keeps asking until a recognised answer is entered.
    std::string answer;
    do {
        std::cout << prompt << " (Y/N): ";
        readConsoleLineOrExit(answer);
        answer = lowerCopy(trim(answer));
        if (answer == "y" || answer == "yes") {
            return true;
        }
        if (answer == "n" || answer == "no") {
            return false;
        }
        std::cout << "Invalid response. Enter Y or N.\n";
    } while (answer != "y" && answer != "yes" && answer != "n" &&
             answer != "no");
    return false;
}

}  // namespace lms
