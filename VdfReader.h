#pragma once
#include "framework.h"
#include <vector>
#include <string>
#include "WinError.h"
#include <functional>

// A reader for Valve's (TM Copyright by Valve, is not mine, me and this code are in no way affiliated with or endorsed by Valve) VDF format,
// used by the Source (TM Copyright by Valve, is not mine, me and this code are in no way affiliated with or endorsed by Valve) engine and
// Steam (TM Copyright by Valve, is not mine, me and this code are in no way affiliated with or endorsed by Valve).
// Example VDF:
// "key"
// {
//   <tab>  "key"  <tab><tab>  "value"
//   <tab>  "key"  <tab><tab>  "value"
//   ...
//   <tab>  "key"
//   <tab>  {
//   <tab><tab>  "key"  <tab><tab>  "value"
//   <tab><tab>  "key"  <tab><tab>  "value"
//   ...
//   <tab>  }
//   <tab>  "key"  <tab><tab>  "value"
//   ...
// }
// 
// Rereads the file every time you call stuff.
// Is this better than storing the parsed file in memory?
// I do not know. Neither approach is very fast for just a one-two calls. And if the file is small it shouldn't matter either way.
class VdfReader
{
public:
	VdfReader();

	// Allows you to open the file immediately upon constructing the object.
	VdfReader(const std::wstring& path);

	// Closes the file upon destruction.
	~VdfReader();

	// Opens a file. Reports error through -winErr- if was unable to open.
	bool open(const std::wstring& path, WinError* winErr = nullptr);

	// Closes the file.
	void close();

	// Obtains a value from a key.
	// -nodePath- denotes a list of N key names, separated by /. The first N-1 key names
	//   are expected to be { ... } objects/dictionaries/lists in the VDF data format.
	//   These objects are expected to be nested inside each other in the order of the keys.
	//   And the last key, the key N, is expected to be the name of the key in the last,
	//   innermost, { ... } object.
	//   That key's value shall be retrieved.
	// If the key or node path are not found, false is returned and -result- will contain an empty string.
	// If the key is found, the function will return true and store the value in the -result-.
	bool getValue(const std::wstring& nodePath, std::wstring& result);
	
	// Checks if the specifies key is present in the VDF file at the given path.
	// -nodePath- denotes a list of N key names, separated by /. All N key names
	//   are expected to be { ... } objects/dictionaries/lists in the VDF data format.
	//   These objects are expected to be nested inside each other in the order of the keys.
	//   It is inside the N'th, innermost, {... } object that the key specified by -key- shall
	//   be checked for its existence.
	// Returns true if the key exists, false otherwise.
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

	// Reads the file until one of the following arises:
	// 1) Error
	// 2) onFinishReadString callback tells it to stop by returning false
	// 3) End of file is reached
	// The onFinishReadString callback can be used to receive strings parsed by the reader.
	// More info about it above.
	bool parse(VdfReaderParseCallback onFinishReadString);
};

