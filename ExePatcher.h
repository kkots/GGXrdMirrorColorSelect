#pragma once
#include "framework.h"
#include <string>
bool performExePatching(const std::wstring& szFile, std::wstring& importantRemarks, HWND mainWindow, bool mayCreateBackup = true);
