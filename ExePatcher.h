#pragma once
#include "framework.h"
#include <string>

/// <summary>
/// Contains hardcode for patching GuiltyGearXrd.exe specifically to do this mirror color select patch.
/// </summary>
/// <param name="szFile">The path to GuiltyGearXrd.exe.</param>
/// <param name="importantRemarks">A string containing "\n" delimited lines of info meant to be shown to the user after completion.
/// Each line is a separate remark.</param>
/// <param name="mainWindow">The window used for displaying message boxes when interaction with the user is required.</param>
/// <param name="mayCreateBackup">true if the function is allowed to perform backup copies of the .exe in case it is not yet patched
/// and a patch application is due.</param>
/// <returns>true on successful patch or if patch is already in place, false on error or cancellation.</returns>
bool performExePatching(const std::wstring& szFile, std::wstring& importantRemarks, HWND mainWindow, bool mayCreateBackup = true);
