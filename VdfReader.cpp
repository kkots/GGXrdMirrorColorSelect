#include "framework.h"
#include "VdfReader.h"
#include <vector>
#include "SharedFunctions.h"

VdfReader::VdfReader() { }

VdfReader::VdfReader(const std::wstring& path) {
	open(path);
}

bool VdfReader::open(const std::wstring& path, WinError* winErr) {
	file = CreateFileW(
			path.c_str(),
			GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		file = NULL;
		WinError err;
		if (winErr) *winErr = err;
		else {
			OutputDebugStringW(L"CreateFileW in VdfReader::open failed: ");
			OutputDebugStringW(err.getMessage());
			OutputDebugStringW(L"\n");
		}
		return false;
	}
	return true;
}

void VdfReader::close() {
	if (file) {
		CloseHandle(file);
		file = NULL;
	}
}

VdfReader::~VdfReader() {
	close();
}

// Splits the -pathStr-, which is expected to be a string of the following format:
//  "keyName1/keyName2/keyName3"
// ..., into a vector of key names:
// 0: keyName1
// 1: keyName2
// 2: keyName3
static std::vector<std::wstring> pathStringToVectorOfString(const std::wstring& pathStr) {
	std::vector<std::wstring> pathVec;
	int lastPos = -1;
	int pos = -1;
	for (wchar_t c : pathStr) {
		++pos;
		if (c == L'/' || c == '\\') {
			if (lastPos == pos - 1) {
				// double slash probably is just a dumb mistake. Ignore
				// this could also be a slash that's the first character in the string. Ignore
				lastPos = pos;
				continue;
			}
			pathVec.emplace_back(pathStr.begin() + (lastPos + 1), pathStr.begin() + pos);
			lastPos = pos;
		}
	}
	if ((int)pathStr.size() > lastPos + 1) {
		pathVec.emplace_back(pathStr.begin() + (lastPos + 1), pathStr.end());
	}
	return pathVec;
}

bool VdfReader::parse(VdfReaderParseCallback onFinishReadString) {
	if (!file) {
		OutputDebugStringW(L"VdfReader::parse: no file open\n");
		return false;
	}

	SetFilePointer(file, 0, NULL, FILE_BEGIN);

	const unsigned int bufferSize = 256;
	char buffer[bufferSize];

	ParsingMode mode = MODE_NONE;
	std::vector<std::wstring> stack;
	std::wstring lastString;
	ParsingMode lastStringType = MODE_NONE;
	std::wstring currentString;
	bool lastCharWasEscapeSlash = false;

	DWORD bytesRead = bufferSize;
	while (bytesRead == bufferSize) {
		if (!ReadFile(file, buffer, bufferSize, &bytesRead, NULL)) {
			WinError err;
			OutputDebugStringW(L"VdfReader::parse: Failed reading file: ");
			OutputDebugStringW(err.getMessage());
			OutputDebugStringW(L"\n");
			return false;
		}
		if (bytesRead == 0) {
			break;
		}
		for (int i = 0; i < (int)bytesRead; ++i) {
			const char c = buffer[i];
			if (mode == MODE_NONE) {
				if (c == '\"') {
					if (lastStringType == MODE_NONE || lastStringType == MODE_VALUE) {
						mode = MODE_KEY;
					} else if (lastStringType == MODE_KEY) {
						mode = MODE_VALUE;
					}
					currentString.clear();
					lastCharWasEscapeSlash = false;
				} else if (c == '{') {
					if (lastStringType == MODE_KEY) {
						stack.push_back(lastString);
						lastString.clear();
						lastStringType = MODE_NONE;
					} else {
						OutputDebugStringW(L"VdfReader::parse: unexpected {\n");
						return false;
					}
				} else if (c == '}') {
					lastString.clear();
					lastStringType = MODE_NONE;
					stack.pop_back();
				} else if (c <= 32) {
					// whitespace
				} else {
					// freak out
					OutputDebugStringW(L"VdfReader::parse: unexpected character: ");
					wchar_t wc[2];
					wc[0] = c;
					wc[1] = L'\0';
					OutputDebugStringW(wc);
					OutputDebugStringW(L"\0");
					return false;
				}
			} else if (c == '\"' && !lastCharWasEscapeSlash) {
				if (!onFinishReadString(stack, lastString, lastStringType, currentString, mode)) {
					return true;
				}
				lastStringType = mode;
				lastString = currentString;
				currentString.clear();
				mode = MODE_NONE;
			} else if (lastCharWasEscapeSlash) {
				currentString.push_back(c);
				lastCharWasEscapeSlash = false;
			} else if (c == '\\') {
				lastCharWasEscapeSlash = true;
			} else {
				currentString.push_back(c);
			}
		}
	}
	return true;
}

bool VdfReader::getValue(const std::wstring& nodePath, std::wstring& result) {
	result.clear();
	std::vector<std::wstring> request = pathStringToVectorOfString(nodePath);
	if (request.empty()) {
		OutputDebugStringW(L"VdfReader::getValue: request is empty\n");
		return false;
	}
	bool success = false;
	int prevNumberOfEqualNodes = 0;
	int numberOfEqualNodes = 0;
	auto callback = [&](
		std::vector<std::wstring>& stack,
		std::wstring& prevTxt,
		ParsingMode prevTxtType,
		std::wstring& currentTxt,
		ParsingMode currentType
	) -> bool {
		if (currentType == MODE_VALUE && prevTxtType == MODE_KEY) {
			stack.push_back(prevTxt);
			if (pathsEqual(stack, request, &numberOfEqualNodes)) {
				result = currentTxt;
				success = true;
				return false;
			}
			if (numberOfEqualNodes < prevNumberOfEqualNodes) {
				return false;
			}
			prevNumberOfEqualNodes = numberOfEqualNodes;
			stack.pop_back();
		}
		return true;
	};
	parse(callback);
	return success;
				
}

bool VdfReader::hasKey(const std::wstring& nodePath, std::wstring& key) {
	std::vector<std::wstring> request = pathStringToVectorOfString(nodePath);
	if (request.empty()) {
		OutputDebugStringW(L"VdfReader::hasKey: request is empty\n");
		return false;
	}
	bool success = false;
	int prevNumberOfEqualNodes = 0;
	int numberOfEqualNodes = 0;
	auto callback = [&](
		std::vector<std::wstring>& stack,
		std::wstring& prevTxt,
		ParsingMode prevTxtType,
		std::wstring& currentTxt,
		ParsingMode currentType
	) -> bool {
		if (currentType == MODE_KEY) {
			if (pathsEqual(stack, request, &numberOfEqualNodes) && key == currentTxt) {
				success = true;
				return false;
			}
			if (numberOfEqualNodes < prevNumberOfEqualNodes) {
				return false;
			}
			prevNumberOfEqualNodes = numberOfEqualNodes;
		}
		return true;
	};
	parse(callback);
	return success;
}
