#pragma once
#include <string>

// Last error produced by either the performUpkPatching, or isUpk or one of the nested calls
const std::wstring& getLastUpkError();

// Checks first 4 bytes and tells if the specified .UPK file is not encrypted.
// If this function encounters any errors, you can obtain them using the getLastUpkError() function.
bool isUpk(const std::wstring& path);

// Performs the hardcoded patch operation on the REDGame.upk file.
// The file must have been decrypted and decompressed beforehand.
// The function does not create any backups and modifies the file in-place.
// If this function encounters any errors, you can obtain them using the getLastUpkError() function.
bool performUpkPatching(const std::wstring& path, bool checkOnly = false, bool* needPatch = nullptr);
