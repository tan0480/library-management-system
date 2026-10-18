// Shared User Interface and Module Integration
// Contributors: All four group members
// Purpose: Displays menus and connects user actions to the four modules.
// Interface responsibilities:
// - Lam Yi Hen: member registration, profile and account screens
// - Tan Yi Heng: book search, borrowing, returns and inventory screens
// - Cheah Yong Shun: fine payment and payment-monitoring screens
// - Lim Jun Yi: report, statistics and report-export screens

#ifndef UI_H
#define UI_H

#include "library_models.h"

namespace lms {

// Runs all role-based menus until the user chooses Exit.
void runConsoleApplication(LibraryData& data);

}  // namespace lms

#endif
