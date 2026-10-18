// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Validates dates and supports due-date and overdue calculations.

#include "date_functions.h"

#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std;

namespace lms {
namespace {

// Returns the number of days in one month for the supplied year.
int daysInMonth(int month, int year) {
    const int monthLengths[12] = {31, 28, 31, 30, 31, 30,
                                  31, 31, 30, 31, 30, 31};
    int numberOfDays = monthLengths[month - 1];
    if (month == 2 && isLeapYear(year)) {
        numberOfDays = 29;
    }
    return numberOfDays;
}

}  // namespace

// Returns true when the supplied year contains 366 days.
bool isLeapYear(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

bool isValidDate(const Date& date) {
    // Check the general ranges before using the month to find its day limit.
    if (date.year < 1900 || date.year > 2200 || date.month < 1 ||
        date.month > 12 || date.day < 1) {
        return false;
    }
    return date.day <= daysInMonth(date.month, date.year);
}

// Converts YYYY-MM-DD text into a Date record passed by reference.
bool parseDate(const std::string& text, Date& date) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }

    for (std::size_t index = 0; index < text.size(); ++index) {
        if (index == 4 || index == 7) {
            continue;
        }
        const unsigned char character =
            static_cast<unsigned char>(text[index]);
        if (std::isdigit(character) == 0) {
            return false;
        }
    }

    Date candidate;
    for (std::size_t index = 0; index < 4; ++index) {
        candidate.year = candidate.year * 10 + (text[index] - '0');
    }
    candidate.month = (text[5] - '0') * 10 + (text[6] - '0');
    candidate.day = (text[8] - '0') * 10 + (text[9] - '0');
    if (!isValidDate(candidate)) {
        return false;
    }
    date = candidate;
    return true;
}

std::string formatDate(const Date& date) {
    // Invalid or unused dates appear as a dash in files and reports.
    if (!isValidDate(date)) {
        return "-";
    }
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << date.year << '-'
           << std::setw(2) << date.month << '-' << std::setw(2) << date.day;
    return output.str();
}

// Converts a date to the number of days since 1 January 1900. Simple loops are
// used so the calculation can be explained using arrays and leap-year rules.
long dateToSerial(const Date& date) {
    long serial = 0;
    for (int year = 1900; year < date.year; ++year) {
        serial += isLeapYear(year) ? 366 : 365;
    }
    for (int month = 1; month < date.month; ++month) {
        serial += daysInMonth(month, date.year);
    }
    serial += date.day - 1;
    return serial;
}

// Converts a day number back into a Date by subtracting whole years and months.
Date serialToDate(long serial) {
    int year = 1900;
    while (serial < 0) {
        --year;
        serial += isLeapYear(year) ? 366 : 365;
    }
    while (serial >= (isLeapYear(year) ? 366 : 365)) {
        serial -= isLeapYear(year) ? 366 : 365;
        ++year;
    }

    int month = 1;
    while (serial >= daysInMonth(month, year)) {
        serial -= daysInMonth(month, year);
        ++month;
    }
    return Date{year, month, static_cast<int>(serial) + 1};
}

// Returns a new date a specified number of days before or after the input date.
Date addDays(const Date& date, int days) {
    return serialToDate(dateToSerial(date) + days);
}

// Returns the number of calendar days from the earlier date to the later date.
int daysBetween(const Date& earlier, const Date& later) {
    return static_cast<int>(dateToSerial(later) - dateToSerial(earlier));
}

Date todayLocal() {
    // The local date is used when new stock advances a reservation queue.
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    return Date{localTime.tm_year + 1900, localTime.tm_mon + 1,
                localTime.tm_mday};
}

// Compares a valid transaction date with the computer's current local date.
bool isFutureDate(const Date& date) {
    return isValidDate(date) &&
           dateToSerial(date) > dateToSerial(todayLocal());
}

}  // namespace lms
