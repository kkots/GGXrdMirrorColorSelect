#pragma once
#include "framework.h"
#include <vector>
#include <string>
#include "WinError.h"
#include <functional>

// Rereads the file every time you call stuff.
// Is this better than storing the parsed file in memory?
// I do not know. Neither approach is very fast for just a one-two calls. And if the file is small it shouldn't matter either way.
class VdfReader
{
public:
	VdfReader();
	VdfReader(const std::wstring& path);
	~VdfReader();
	bool open(const std::wstring& path, WinError* winErr = nullptr);
	void close();
	bool getValue(const std::wstring& nodePath, std::wstring& result);
	bool hasKey(const std::wstring& nodePath, std::wstring& key);
	enum ParsingMode {
		MODE_NONE,
		MODE_KEY,
		MODE_VALUE
	};
private:
	HANDLE file = NULL;

	/* This callback is called when the -parse- method finishes reading a string.
	* stack - array of node names from outer to inner, not including the current key.
	* prevTxt - the string that was read before this. If currentTxt is the first one in
	*           a { .. } object/dictionary, prevTxt is empty and its type is MODE_NONE.
	*           If currentTxt is a key, prevTxt and its type are undefined.
	*           If currentTxt is a value, prevTxt is the key.
	* prevTxtType - the type of prevTxt. See prevTxt description.
	* currentTxt - the current string. See prevTxt description.
	* currentType - the type of the current string. See prevTxt description.
	* Return - true to keep reading the file, false if you want to stop reading it prematurely. */
	typedef std::function<bool(
		std::vector<std::wstring>& stack,
		std::wstring& prevTxt,
		ParsingMode prevTxtType,
		std::wstring& currentTxt,
		ParsingMode currentType
	)> VdfReaderParseCallback;

	bool parse(VdfReaderParseCallback onFinishReadString);
};

