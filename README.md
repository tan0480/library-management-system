# Library Management System

A C++17 console application for managing library members, books, borrowing, returns, reservations, payments and reports. Records are stored in local text files.

## Features

- Member registration, login, profile updates, search and deactivation.
- Book catalogue management, search and inventory tracking.
- Borrowing and returns with loan limits, deposits and overdue fines.
- Payment records and reservation queues.
- Member, inventory, borrowing, overdue, payment and category reports, with text-file export.

## Build and run

Use a C++17 compiler with standard filesystem support. On Windows, install the MSYS2 UCRT64 GCC toolchain and make its `bin` directory available on `PATH` for both compilation and execution.

Run these commands in PowerShell from the repository root:

```powershell
New-Item -ItemType Directory -Force -Path build | Out-Null
$sources = @(Get-ChildItem -LiteralPath src -File | Where-Object Extension -eq '.cpp' | ForEach-Object FullName)
g++ -std=c++17 -Wall -Wextra -Wpedantic -I include @sources -o build/LibraryManagementSystem.exe
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed' }
.\build\LibraryManagementSystem.exe
```

Keep running the application from the repository root. On a fresh checkout, it creates a `data` directory with demonstration records, including 150 books. It uses five files: `Member.txt`, `Book.txt`, `BorrowingHistory.txt`, `Reservation.txt` and `Payment.txt`.

The demonstration librarian login is `admin` / `admin123`. Fresh sample member accounts are `M001` and `M002`, both with password `pass123`.

Existing personal data is intentionally not included in this repository. Back up any local data before replacing it or testing operations that change records.

## Source layout

- `include/`: shared data structures and module interfaces (13 headers).
- `src/`: module implementations and console entry point (13 C++ files).
- `src/main.cpp`: data initialization and application startup.
- `src/ui.cpp`: console menus and module integration.

## Coding contributions

| Contributor | Main coding contribution |
| --- | --- |
| [Tan Yi Heng](https://github.com/tan0480) | Book management and search, borrowing, returns, inventory updates and related validation/UI integration. |
| [Lam Yi Hen](https://github.com/lamyihen77-beep) | Member registration, authentication, profile management, search and deactivation. |
| [Cheah Yong Shun](https://github.com/ys012007) | Payment utilities, fines, deposits, refunds and payment workflows. |
| [Lim Jun Yi](https://github.com/ljunyi0212) | Reporting, statistics and report exports. |

Shared models, storage, date/string/input/search utilities, reservations and console integration are attributed jointly to all four contributors. The borrowing/return implementation also contains fine and payment work shared by Tan Yi Heng and Cheah Yong Shun.

This repository imports an existing coursework project. Import commits identify the contributors to the imported code; they do not reconstruct the original development timeline. Tan Yi Heng prepares and pushes the imports, with shared work recorded through co-author trailers.

## Limitations and excluded files

This is an academic demonstration, not a production library or payment service. The administrator credentials are fixed in the source, and member passwords are stored in plaintext. Do not use real passwords, real payment details or sensitive personal information. Payment options are simulations, not a connection to a payment processor.

Local data, IDE configuration, binaries, build output, temporary files, PDFs and generated root-level text reports are excluded from version control. Only the source, headers and repository documentation are included.
