#pragma once
#include <string>
const std::wstring& getLastUpkError();
bool isUpk(const std::wstring& path);
bool performUpkPatching(const std::wstring& path, bool checkOnly = false, bool* needPatch = nullptr);