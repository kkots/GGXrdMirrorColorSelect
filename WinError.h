#pragma once
#include "framework.h"
// This is an exact copy-paste of the WinError.h file from ggxrd_hitbox_injector project

// Class for simplifying the calls to GetLastError() and obtaining a Windows error message.
// Calls GetLastError() as soon as it is constructed and lazy-obtains the message (meaning
// it is requested from the OS only when needed) which corresponds to that error code.
// Usage:
//   some code that errs, for example:
//   if (!DeleteFileW(specify a directory here)) {
//      WinError winErr;  // calls GetLastError()
//      std::wcout << winErr.getMessage() << std::endl; // obtains error message
//   }
class WinError {
public:
    LPWSTR message = NULL;
    DWORD code = 0;
    WinError();
    void moveFrom(WinError& src) noexcept;
    void copyFrom(const WinError& src);
    WinError(const WinError& src);
    WinError(WinError&& src) noexcept;
    LPCWSTR getMessage();
    void clear();
    ~WinError();
    WinError& operator=(const WinError& src);
    WinError& operator=(WinError&& src) noexcept;
};
