// Shared Integration/Utility Module
// Contributors: All four group members
// Purpose: Loads the shared data and starts the menu-driven application.

#include "storage.h"
#include "ui.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

using namespace std;

namespace fs = std::filesystem;

// Future enhancements outside this diploma assignment include password hashing,
// account recovery, database storage, concurrency and advanced encryption.
namespace {

// Inspects required paths so unreadable files are never mistaken for missing files.
bool requiredDataFileCount(const std::string& directory, int& regularFileCount,
                           int& existingPathCount, std::string& message) {
    const std::string fileNames[5] = {
        "Member.txt", "Book.txt", "BorrowingHistory.txt",
        "Reservation.txt", "Payment.txt"};
    regularFileCount = 0;
    existingPathCount = 0;
    for (int index = 0; index < 5; ++index) {
        const fs::path path = fs::path(directory) / fileNames[index];
        std::error_code error;
        const bool exists = fs::exists(path, error);
        if (error) {
            message = "Filesystem inspection failed for " + path.string() +
                      ": " + error.message() + ".";
            return false;
        }
        if (exists) {
            ++existingPathCount;
            const bool regularFile = fs::is_regular_file(path, error);
            if (error) {
                message = "Filesystem inspection failed for " + path.string() +
                          ": " + error.message() + ".";
                return false;
            }
            if (regularFile) {
                ++regularFileCount;
            }
        }
    }
    return true;
}

// Supports launching from either the project root or its build directory.
bool selectDataDirectory(std::string& directory, std::string& message) {
    int projectRegularFiles = 0;
    int projectExistingPaths = 0;
    int parentRegularFiles = 0;
    int parentExistingPaths = 0;
    if (!requiredDataFileCount("data", projectRegularFiles,
                               projectExistingPaths, message) ||
        !requiredDataFileCount("../data", parentRegularFiles,
                               parentExistingPaths, message)) {
        return false;
    }

    if (projectRegularFiles == 5) {
        directory = "data";
        return true;
    }
    if (parentRegularFiles == 5) {
        directory = "../data";
        return true;
    }
    // Preserve and report a partial set instead of selecting an empty folder.
    if (projectExistingPaths > 0) {
        directory = "data";
        return true;
    }
    if (parentExistingPaths > 0) {
        directory = "../data";
        return true;
    }
    directory = "data";
    return true;
}

}  // namespace

// Starts the system using the first suitable data folder.
int main() {
    // LibraryData is the shared record collection passed to every program module.
    lms::LibraryData data;
    std::string message;
    std::string dataDirectory;
    if (!selectDataDirectory(dataDirectory, message)) {
        std::cerr << "Unable to start the system: " << message << '\n';
        return 1;
    }
    if (!lms::initializeLibrary(data, dataDirectory, message)) {
        std::cerr << "Unable to start the system: " << message << '\n';
        return 1;
    }

    std::cout << message << '\n';
    lms::runConsoleApplication(data);
    return 0;
}
