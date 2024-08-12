#pragma once
#include "framework.h"
#include <string>
#include <vector>
int findCharRevW(const wchar_t* buf, wchar_t c);
int findLast(const std::wstring& str, wchar_t character);
bool fileExists(LPCWSTR path);
bool fileExists(const std::wstring& path);
bool folderExists(LPCWSTR path);
bool folderExists(const std::wstring& path);
bool fileOrFolderExists(LPCWSTR path);
bool fileOrFolderExists(const std::wstring& path);
std::wstring strToWStr(const std::string& str);
std::wstring getFileName(const std::wstring& path);
std::wstring getParentDir(const std::wstring& path);
std::wstring generateUniqueBackupName(const std::wstring& path);
bool pathsEqual(const std::vector<std::wstring>& pathA, const std::vector<std::wstring>& pathB, int* numberOfEqualNodes = nullptr);