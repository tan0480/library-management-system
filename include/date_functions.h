// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Validates dates and supports due-date and overdue calculations.

#ifndef DATE_FUNCTIONS_H
#define DATE_FUNCTIONS_H

#include "library_models.h"

#include <string>

namespace lms {

// Returns true when the supplied year contains 366 days.
bool isLeapYear(int year);
// Returns true when every part of a Date record is valid.
bool isValidDate(const Date& date);
// Converts YYYY-MM-DD text into a Date record.
bool parseDate(const std::string& text, Date& date);
// Converts a valid Date record into YYYY-MM-DD text.
std::string formatDate(const Date& date);
// Converts a Date into a day number for calculations.
long dateToSerial(const Date& date);
// Converts a day number back into a Date record.
Date serialToDate(long serial);
// Returns a new Date after adding or subtracting days.
Date addDays(const Date& date, int days);
// Returns the number of days between two dates.
int daysBetween(const Date& earlier, const Date& later);
// Returns the current local calendar date.
Date todayLocal();
// Returns true when a valid supplied date is later than today's local date.
bool isFutureDate(const Date& date);

}  // namespace lms

#endif
