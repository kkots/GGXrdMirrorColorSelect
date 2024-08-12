#pragma once
#include "framework.h"
#include <string>
#include <vector>

// Find a character starting from the end of the string.
// Returns the last position of the character. So 0 would be the start of the string.
// Returns -1 if the character is not found.
int findCharRevW(const wchar_t* buf, wchar_t c);

// Find a character starting from the end of the string.
// Returns the last position of the character. So 0 would be the start of the string.
// Returns -1 if the character is not found.
int findLast(const std::wstring& str, wchar_t character);

// Checks if the file specified by -path- exists and is a file.
// Returns false if the file doesn't exist or is a directory.
bool fileExists(LPCWSTR path);

// Checks if the file specified by -path- exists and is a file.
// Returns false if the file doesn't exist or is a directory.
bool fileExists(const std::wstring& path);

// Checks if the directory specified by -path- exists and is a directory.
// Returns false if the directory doesn't exist or is not a directory.
bool folderExists(LPCWSTR path);

// Checks if the directory specified by -path- exists and is a directory.
// Returns false if the directory doesn't exist or is not a directory.
bool folderExists(const std::wstring& path);

// Checks if the file or directory specified by -path- exists.
bool fileOrFolderExists(LPCWSTR path);

// Checks if the file or directory specified by -path- exists.
bool fileOrFolderExists(const std::wstring& path);

// Converts a single-byte string to a wide-character (2 byte) string without
// performing any encoding or decoding. The characters are simply widened directly.
// This function cannot be used to convert non-ASCII, multi-language UTF-8 strings to
// UTF-16 wide-character strings.
std::wstring strToWStr(const std::string& str);

// Returns the filename portion of the file specified by the provided -path-.
// Includes the file name extension.
std::wstring getFileName(const std::wstring& path);

// Returns the parent directory of the file specified by the provided -path-.
// Does not include the trailing \.
std::wstring getParentDir(const std::wstring& path);

// Generates a name for the file specified by -path- with a "_backup" or "_backup#" suffix,
// where # is a number selected by the algorithm in such a way so as to form a unique filename.
// The returned path is in the same directory as the original file.
// No file is actually created, this function only generates a name.
// If multiple programs are using this folder it is possible that the name is occupied by
// the time you attempt to use it.
// Consider using GetTempFileNameW if you want to generate a unique temporary file name instead.
std::wstring generateUniqueBackupName(const std::wstring& path);

// Compares strings between pathA and pathB.
// Returns true only if all strings between A and B match in the exact order.
// Outputs the count of equal strings into the -numberOfEqualNodes- pointer.
//   Only the first equal strings are considered equal. So for example:
//   In X Y Z W == X Y P W,
//   Only 2 strings would be equal, not 3.
//   And in X Y Z W == X Y Z P,
//   3 strings would be equal.
bool pathsEqual(const std::vector<std::wstring>& pathA, const std::vector<std::wstring>& pathB, int* numberOfEqualNodes = nullptr);
