
#include "UpkPatcher.h"
#include "framework.h"
#include "SharedFunctions.h"
#include <vector>
#include <string>
#include <io.h>     // for _open_osfhandle
#include <fcntl.h>  // for _O_RDONLY
#include "WinError.h"

#define PACKAGE_FILE_TAG			0x9E2A83C1

// for ""s literals
using namespace std::literals;

// Last error produced by either the performUpkPatching, or isUpk or one of the nested calls
static std::wstring lastError;
// Buffer used for all sprintf_s calls
static char sprintfbuf[1024] { 0 };

const std::wstring& getLastUpkError() {
	return lastError;
}

// The dividend is the number that we divide, and the divisor is what we divide it by
// This function rounds the division result up if it isn't whole
static int divideIntButRoundUp(int dividend, int divisor) {
	int mod = dividend % divisor;
	if (mod) {
		return dividend / divisor + 1;
	} else {
		return dividend / divisor;
	}
}

// byteSpecification is in hex. Example: "00 af de"
// Appends bytes specified by -byteSpecification- to the end of -vec-.
static void pushIntoUCharVector(std::vector<unsigned char>& vec, const char* byteSpecification) {
	vec.reserve(vec.size() + divideIntButRoundUp((int)strlen(byteSpecification), 2));
	unsigned long long accumulatedNibbles = 0;
	int nibbleCount = 0;
	const char* byteSpecificationPtr = byteSpecification;
	while (true) {
		char currentChar = *byteSpecificationPtr;
		if (currentChar != ' ' && currentChar != '\0') {
			char currentNibble = 0;
			if (currentChar >= '0' && currentChar <= '9') {
				currentNibble = currentChar - '0';
			} else if (currentChar >= 'a' && currentChar <= 'f') {
				currentNibble = currentChar - 'a' + 10;
			} else if (currentChar >= 'A' && currentChar <= 'F') {
				currentNibble = currentChar - 'A' + 10;
			} else {
				std::wstring errorMsg = L"Wrong byte specification: " + strToWStr(byteSpecification);
				MessageBoxW(NULL, errorMsg.c_str(), L"Error", MB_OK);
				throw errorMsg;
			}
			accumulatedNibbles = (accumulatedNibbles << 4) | currentNibble;
			++nibbleCount;
			if (nibbleCount > 16) {
				std::wstring errorMsg = L"Wrong byte specification: " + strToWStr(byteSpecification);
				MessageBoxW(NULL, errorMsg.c_str(), L"Error", MB_OK);
				throw errorMsg;
			}
		} else if (nibbleCount) {
			do {
				vec.push_back(accumulatedNibbles & 0xff);
				accumulatedNibbles >>= 8;
				nibbleCount -= 2;
			} while (nibbleCount > 0);
			nibbleCount = 0;
			if (currentChar == '\0') {
				break;
			}
		}
		++byteSpecificationPtr;
	}
}

// byteSpecification is in hex. Example: "00 af de"
// Compares bytes between -vec- and -byteSpecification-.
// vecEnd specifies the end of the vector to which the -vec- iterator belongs,
//  so as not to go out of bounds. It does not mean the end of the region to check for equality.
//  Reaching -vecEnd- prematurely would mean the bytes are not equal.
static bool bytesEqual(std::vector<unsigned char>::iterator vec, std::vector<unsigned char>::iterator vecEnd,
					const char* byteSpecification) {
	if (vec == vecEnd) return false;
	unsigned long long accumulatedNibbles = 0;
	int nibbleCount = 0;
	bool nibblesUnknown = false;
	const char* byteSpecificationPtr = byteSpecification;
	while (true) {
		char currentChar = *byteSpecificationPtr;
		if (currentChar != ' ' && currentChar != '\0') {
			char currentNibble = 0;
			if (currentChar >= '0' && currentChar <= '9' && !nibblesUnknown) {
				currentNibble = currentChar - '0';
			} else if (currentChar >= 'a' && currentChar <= 'f' && !nibblesUnknown) {
				currentNibble = currentChar - 'a' + 10;
			} else if (currentChar >= 'A' && currentChar <= 'F' && !nibblesUnknown) {
				currentNibble = currentChar - 'A' + 10;
			} else if (currentChar == '?' && (nibbleCount == 0 || nibblesUnknown)) {
				nibblesUnknown = true;
			} else {
				std::wstring errorMsg = L"Wrong byte specification: " + strToWStr(byteSpecification);
				MessageBoxW(NULL, errorMsg.c_str(), L"Error", MB_OK);
				throw errorMsg;
			}
			accumulatedNibbles = (accumulatedNibbles << 4) | currentNibble;
			++nibbleCount;
			if (nibbleCount > 16) {
				std::wstring errorMsg = L"Wrong byte specification: " + strToWStr(byteSpecification);
				MessageBoxW(NULL, errorMsg.c_str(), L"Error", MB_OK);
				throw errorMsg;
			}
		} else if (nibbleCount) {
			do {
				if (vec == vecEnd) {
					return false;
				}
				if (!nibblesUnknown) {
					unsigned char vecVal = *vec;
					if (vecVal != (accumulatedNibbles & 0xff)) {
						return false;
					}
					accumulatedNibbles >>= 8;
				}
				++vec;
				nibbleCount -= 2;
			} while (nibbleCount > 0);
			nibbleCount = 0;
			nibblesUnknown = false;
			if (currentChar == '\0') {
				break;
			}
		}
		++byteSpecificationPtr;
	}
	return true;
}

// Overwrites elements in -dst- using elements from -src- without shifting any elements in -dst-.
// -end- specifies the end of the vector to which the -dst- iterator belongs.
//  It does not mean the end of the region to overwrite and is only used for bounds checking.
//  Reaching the -end- prematurely will stop the overwrite operation.
static void overwriteUCharVector(std::vector<unsigned char>::iterator dst, std::vector<unsigned char>::iterator end, std::vector<unsigned char>& src) {
	for (unsigned char c : src) {
		if (dst == end) return;
		*dst = c;
		++dst;
	}
}

// Overwrites elements in -dst- using -count- characters -c- without shifting any elements in -dst-.
// -end- specifies the end of the vector to which the -dst- iterator belongs.
//  It does not mean the end of the region to overwrite and is only used for bounds checking.
//  Reaching the -end- prematurely will stop the overwrite operation.
static void overwriteUCharVector(std::vector<unsigned char>::iterator dst, std::vector<unsigned char>::iterator end, int count, unsigned int c) {
	while (count) {
		--count;
		if (dst == end) return;
		*dst = c;
		++dst;
	}
}

// Overwrites 4 elements in -dst- with the unsigned int -val- in Little Endian format without shifting any elements in -dst-.
// -end- specifies the end of the vector to which the -dst- iterator belongs.
//  It does not mean the end of the region to overwrite and is only used for bounds checking.
//  Reaching the -end- prematurely will stop the overwrite operation.
static void overwriteUCharVector(std::vector<unsigned char>::iterator dst, std::vector<unsigned char>::iterator end, unsigned int val) {
	while (val) {
		if (dst == end) return;
		*dst = (val & 0xff);
		val >>= 8;
		++dst;
	}
}

// Overwrites 4 elements in -dst- with the int -val- in Little Endian format without shifting any elements in -dst-.
// -end- specifies the end of the vector to which the -dst- iterator belongs.
//  It does not mean the end of the region to overwrite and is only used for bounds checking.
//  Reaching the -end- prematurely will stop the overwrite operation.
static void overwriteUCharVector(std::vector<unsigned char>::iterator dst, std::vector<unsigned char>::iterator end, int val) {
	overwriteUCharVector(dst, end, (unsigned int)val);
}

// Reads 4 elements from -dst- as one unsigned int and returns the result. The unsigned int is assumed to be stored in Little Endian format.
// -end- specifies the end of the vector to which the -dst- iterator belongs.
//  It does not mean the end of the region to read from and is only used for bounds checking.
//  Reaching the -end- prematurely will stop the read operation.
static unsigned int readFromUCharArray(std::vector<unsigned char>::iterator dst, std::vector<unsigned char>::iterator end) {
	unsigned int v = 0;
	for (int i = 0; i < 4; ++i) {
		if (dst == end) break;
		v |= (*dst) << (i * 8);
		++dst;
	}
	return v;
}

// Reads a serialized FString from the -file-.
static void readString(std::wstring& str, FILE* file) {
	int length;
	fread(&length, 4, 1, file);
	if (length > 0) {
		std::string strSingleByte(length - 1, '\0');
		fread(&strSingleByte.front(), 1, length, file);
		str.reserve(length - 1);
		for (char c : strSingleByte) {
			str.push_back((wchar_t)c);
		}
	} else {
		str.reserve(-length - 1);
		fread(&str.front(), 2, -length - 1, file);
	}
}

// A representation of an FName
struct NameData {
	std::wstring name;
	int numberPart = 0;
};

// Reads a serialized FName from the -file-.
// The nameMap is used to immediately convert the base part of the name to a string.
NameData readNameData(std::vector<std::wstring>& nameMap, FILE* file) {
	NameData newData;
	int nameIndex;
	fread(&nameIndex, 4, 1, file);
	if (nameIndex < 0 || nameIndex >= (int)nameMap.size()) {
		sprintf_s(sprintfbuf, "Name index %d outside the range [0;%d)\n", nameIndex, (int)nameMap.size());
		lastError = strToWStr(sprintfbuf);
		return NameData{};
	}
	newData.name = nameMap[nameIndex];
	fread(&newData.numberPart, 4, 1, file);
	return newData;
}

// Creates a string representation of the provided -nameData- by combining the base name part with the number part.
std::wstring nameDataToString(NameData& nameData) {
	std::wstring result = nameData.name;
	if (nameData.numberPart) {
		result += L'_';
		unsigned __int64 numberPart64 = nameData.numberPart - 1;
		char buffer[25]{ '\0' };
		_ui64toa_s(numberPart64, buffer, sizeof buffer, 10);
		result.reserve(result.size() + strlen(buffer));
		for (char* c = buffer; *c != '\0' && c - buffer <= sizeof buffer; ++c) {
			result += (wchar_t)*c;
		}
	}
	return result;
}

struct UEGuid {
	DWORD a = 0;
	DWORD b = 0;
	DWORD c = 0;
	DWORD d = 0;
};

// Reads a serialized FGuid from the -file-.
void readGuid(UEGuid& guid, FILE* file) {
	fread(&guid.a, 4, 1, file);
	fread(&guid.b, 4, 1, file);
	fread(&guid.c, 4, 1, file);
	fread(&guid.d, 4, 1, file);
}

// Checks first 4 bytes and tells if the specified .UPK file is not encrypted.
// If this function encounters any errors, you can obtain them using the getLastUpkError() function.
bool isUpk(const std::wstring& path) {
	HANDLE fileHandle = CreateFileW(
		path.c_str(),
		GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!fileHandle || fileHandle == INVALID_HANDLE_VALUE) {
		WinError winErr;
		lastError = L"Error opening " + path + L": " + winErr.getMessage();
		return false;
	}
	bool result = false;
	int tag;
	DWORD bytesRead = 0;
	if (!ReadFile(fileHandle, &tag, 4, &bytesRead, NULL)) {
		WinError winErr;
		lastError = L"Error reading " + path + L": " + winErr.getMessage();
		bytesRead = 0;
	}
	if (bytesRead == 4) {
		result = (tag == PACKAGE_FILE_TAG);
	}
	CloseHandle(fileHandle);
	return result;
}

// Performs the hardcoded patch operation on the REDGame.upk file.
// The file must have been decrypted and decompressed beforehand.
// The function does not create any backups and modifies the file in-place.
// If this function encounters any errors, you can obtain them using the getLastUpkError() function.
bool performUpkPatching(const std::wstring& path, bool checkOnly, bool* needPatch)
{
	if (needPatch) *needPatch = false;
	std::wstring fileName = getFileName(path);
	std::vector<std::wstring> nodeToSeek;
	nodeToSeek.push_back(L"REDGfxMoviePlayer_MenuCharaSelect_AC20");

	struct CloseFilesAtTheEnd {
	public:
		~CloseFilesAtTheEnd() {
			if (file) fclose(file);
		}
		FILE* file = nullptr;
	} closeFilesAtTheEnd;

	HANDLE fileHandle = CreateFileW(
		path.c_str(),
		checkOnly ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
		checkOnly ? FILE_SHARE_READ : FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		WinError winErr;
		lastError = L"Failed to open file: "s + winErr.getMessage();
		return false;
	}
	DWORD bytesWritten = 0;
	int fileDesc = _open_osfhandle((intptr_t)fileHandle, checkOnly ? _O_RDONLY : _O_RDWR);
	FILE* file = _fdopen(fileDesc, checkOnly ? "rb" : "r+b");
	closeFilesAtTheEnd.file = file;
	int tag;
	fread(&tag, 4, 1, file);
	if (tag != PACKAGE_FILE_TAG) {
		lastError = L"Package file tag doesn't match.";
		return false;
	}
	int fileVersion;
	fread(&fileVersion, 4, 1, file);
	int totalHeaderSize;
	fread(&totalHeaderSize, 4, 1, file);
	std::wstring folderName;
	readString(folderName, file);
	DWORD packageFlags;
	fread(&packageFlags, 4, 1, file);
	int nameCount;
	fread(&nameCount, 4, 1, file);
	int nameOffset;
	fread(&nameOffset, 4, 1, file);
	int exportCount;
	fread(&exportCount, 4, 1, file);
	int exportOffset;
	fread(&exportOffset, 4, 1, file);
	int importCount;
	fread(&importCount, 4, 1, file);
	int importOffset;
	fread(&importOffset, 4, 1, file);
	int dependsOffset;
	fread(&dependsOffset, 4, 1, file);
	int importExportGuidOffsets = -1;
	int importGuidsCount = 0;
	int exportGuidsCount = 0;
	if ((fileVersion & 0xffff) >= 623) {
		fread(&importExportGuidOffsets, 4, 1, file);
		fread(&importGuidsCount, 4, 1, file);
		fread(&exportGuidsCount, 4, 1, file);
	}
	int thumbnailTableOffset = 0;
	if ((fileVersion & 0xffff) >= 584) {
		fread(&thumbnailTableOffset, 4, 1, file);
	}
	UEGuid guid;
	readGuid(guid, file);
	int generationCount;
	fread(&generationCount, 4, 1, file);
	if (generationCount) {
		for (int generationCounter = generationCount; generationCounter > 0; --generationCounter) {
			int generationExportCount;
			int generationNameCount;
			int generationNetObjectCount;
			fread(&generationExportCount, 4, 1, file);
			fread(&generationNameCount, 4, 1, file);
			fread(&generationNetObjectCount, 4, 1, file);
		}
	}
	int engineVersion;
	fread(&engineVersion, 4, 1, file);
	int cookedContentVersion;
	fread(&cookedContentVersion, 4, 1, file);
	DWORD compressionFlags;
	fread(&compressionFlags, 4, 1, file);
	int compressedChunksCount;
	fread(&compressedChunksCount, 4, 1, file);
	if (compressedChunksCount > 0) {
		for (int compressedChunksCounter = compressedChunksCount; compressedChunksCounter > 0; --compressedChunksCounter) {
			int uncompressedOffset;
			int uncompressedSize;
			int compressedOffset;
			int compressedSize;
			fread(&uncompressedOffset, 4, 1, file);
			fread(&uncompressedSize, 4, 1, file);
			fread(&compressedOffset, 4, 1, file);
			fread(&compressedSize, 4, 1, file);
		}
	}
	DWORD packageSource;
	fread(&packageSource, 4, 1, file);
	if ((fileVersion & 0xffff) >= 516) {
		int additionalPackagesToCookCount;
		fread(&additionalPackagesToCookCount, 4, 1, file);
		if (additionalPackagesToCookCount > 0) {
			for (int additionalPackagesToCookCounter = additionalPackagesToCookCount; additionalPackagesToCookCounter > 0; --additionalPackagesToCookCounter) {
				std::wstring packageName;
				readString(packageName, file);
			}
		}
	}
	if ((fileVersion & 0xffff) >= 767) {
		int textureTypesCount;
		fread(&textureTypesCount, 4, 1, file);
		if (textureTypesCount > 0) {
			for (int textureTypesCounter = textureTypesCount; textureTypesCounter > 0; --textureTypesCounter) {
				int sizeX;
				int sizeY;
				int numMips;
				DWORD format;
				DWORD texCreateFlags;
				int exportIndicesCount;
				fread(&sizeX, 4, 1, file);
				fread(&sizeY, 4, 1, file);
				fread(&numMips, 4, 1, file);
				fread(&format, 4, 1, file);
				fread(&texCreateFlags, 4, 1, file);
				fread(&exportIndicesCount, 4, 1, file);
				if (exportIndicesCount > 0) {
					for (int exportIndicesCounter = exportIndicesCount; exportIndicesCounter > 0; --exportIndicesCounter) {
						int exportIndex;
						fread(&exportIndex, 4, 1, file);
					}
				}
			}
		}
	}
	if (compressionFlags != 0) {
		lastError = L"The .UPK is still compressed after running the decompress.exe tool. Cannot continue.";
		return false;
	}
	std::vector<std::wstring> names;
	if (nameCount) {
		fseek(file, nameOffset, SEEK_SET);
		for (int nameCounter = nameCount; nameCounter > 0; --nameCounter) {
			std::wstring name;
			readString(name, file);
			names.push_back(name);
			unsigned long long contextFlags;
			fread(&contextFlags, 8, 1, file);
		}
	}
	std::vector<std::wstring> importNames;
	if (importCount) {
		fseek(file, importOffset, SEEK_SET);
		for (int importCounter = importCount; importCounter > 0; --importCounter) {
			NameData classPackage = readNameData(names, file);
			NameData className = readNameData(names, file);
			fseek(file, 4, SEEK_CUR);
			NameData objectName = readNameData(names, file);
			importNames.push_back(nameDataToString(objectName));
		}
		if (!lastError.empty()) return false;
		fseek(file, importOffset, SEEK_SET);
		for (int importCounter = importCount; importCounter > 0; --importCounter) {
			NameData classPackage = readNameData(names, file);
			NameData className = readNameData(names, file);
			int outerIndex;
			fread(&outerIndex, 4, 1, file);
			if (outerIndex > 0) {
				lastError = L"Error: outer index in imports points to exports.\n";
				return false;
			}
			NameData objectName = readNameData(names, file);
		}
		if (!lastError.empty()) return false;
	}
	struct Export {
		int filePositionForSizeAndOffset = 0;
		int serialOffset = 0;
		int serialSize = 0;
		std::wstring name;
		std::vector<std::wstring> packagePath;
		std::wstring className;
		int outerIndex = 0;
	};
	Export* Player_UpdateCustomMenuExportsPtr = nullptr;
	std::vector<Export> exports;
	if (exportCount) {
		fseek(file, exportOffset, SEEK_SET);
		for (int exportCounter = exportCount; exportCounter > 0; --exportCounter) {
			fseek(file, 12, SEEK_CUR);
			NameData objectName = readNameData(names, file);
			if (!lastError.empty()) return false;
			Export newExport;
			newExport.name = nameDataToString(objectName);
			exports.push_back(newExport);
			fseek(file, 24, SEEK_CUR);
			int generationNetObjectCountCount;
			fread(&generationNetObjectCountCount, 4, 1, file);
			if (generationNetObjectCountCount > 0) {
				fseek(file, generationNetObjectCountCount * 4, SEEK_CUR);
			}
			fseek(file, 20, SEEK_CUR);
		}
		fseek(file, exportOffset, SEEK_SET);
		for (int exportCounter = 0; exportCounter < exportCount; ++exportCounter) {
			Export& exportStruct = exports[exportCounter];
			int classIndex;
			fread(&classIndex, 4, 1, file);
			if (classIndex) {
				if (classIndex < 0) {
					exportStruct.className = importNames[-classIndex - 1];
				} else {
					exportStruct.className = exports[classIndex - 1].name;
				}
			}
			int superIndex;
			fread(&superIndex, 4, 1, file);
			int outerIndex;
			fread(&outerIndex, 4, 1, file);
			exportStruct.outerIndex = outerIndex;
			NameData objectName = readNameData(names, file);
			if (!lastError.empty()) return false;
			int archetypeIndex;
			fread(&archetypeIndex, 4, 1, file);
			unsigned long long objectFlags;
			fread(&objectFlags, 8, 1, file);
			exportStruct.filePositionForSizeAndOffset = ftell(file);
			fread(&exportStruct.serialSize, 4, 1, file);
			fread(&exportStruct.serialOffset, 4, 1, file);
			if ((fileVersion & 0xffff) < 543) {
				int len;
				fread(&len, 4, 1, file);
				fseek(file, len * 3 * 4, SEEK_CUR);
			}
			DWORD exportFlags;
			fread(&exportFlags, 4, 1, file);
			int generationNetObjectCountCount;
			fread(&generationNetObjectCountCount, 4, 1, file);
			if (generationNetObjectCountCount > 0) {
				for (int generationNetObjectCountCounter = generationNetObjectCountCount; generationNetObjectCountCounter > 0; --generationNetObjectCountCounter) {
					int count;
					fread(&count, 4, 1, file);
				}
			}
			UEGuid exportGuid;
			readGuid(exportGuid, file);
			DWORD exportPackageFlags;
			fread(&exportPackageFlags, 4, 1, file);
		}
		for (int exportCounter = 0; exportCounter < exportCount; ++exportCounter) {
			Export& exportStruct = exports[exportCounter];
			int outerIndexIter = exportStruct.outerIndex;
			while (outerIndexIter) {
				if (outerIndexIter < 0) {
					break;
				} else {
					exportStruct.packagePath.push_back(exports[outerIndexIter - 1].name);
					outerIndexIter = exports[outerIndexIter - 1].outerIndex;
				}
			}
			std::reverse(exportStruct.packagePath.begin(), exportStruct.packagePath.end());
			if (pathsEqual(exportStruct.packagePath, nodeToSeek, nullptr)
					&& exportStruct.name == L"Player_UpdateCustomMenu") {
				Player_UpdateCustomMenuExportsPtr = &exportStruct;
				break;
			}
		}
	}

	if (!Player_UpdateCustomMenuExportsPtr) {
		lastError = L"Could not find Player_UpdateCustomMenu in the exports table in the UPK.";
		return false;
	}

	fseek(file, Player_UpdateCustomMenuExportsPtr->serialOffset, SEEK_SET);

	std::vector<unsigned char> copyBuf(Player_UpdateCustomMenuExportsPtr->serialSize);
	size_t bytesRead = fread(copyBuf.data(), 1, Player_UpdateCustomMenuExportsPtr->serialSize, file);
	if (bytesRead != Player_UpdateCustomMenuExportsPtr->serialSize) {
		lastError = L"Failed to read Player_UpdateCustomMenu function.";
		return false;
	}

	fseek(file, 0, SEEK_END);
	int newFilePositionForObject = ftell(file);

	if (copyBuf.size() < 0xca4 + 0x30) {
		lastError = L"The discovered Player_UpdateCustomMenu function is too small.";
		return false;
	}
	auto codeStart = copyBuf.begin() + 0x30;
	std::vector<unsigned char> ParamSelectColor_bStructWillBeModified_0(codeStart + 0x3ba, codeStart + 0x3ca);
	std::vector<unsigned char> ParamSelectColor_bStructWillBeModified_1 = ParamSelectColor_bStructWillBeModified_0;
	ParamSelectColor_bStructWillBeModified_0[10] = 0;
	ParamSelectColor_bStructWillBeModified_1[10] = 1;
	std::vector<unsigned char> GetColorMax(codeStart + 0x560, codeStart + 0x56a);

	// This is a copy of the patch below
	if (bytesEqual(codeStart + 0x3a4, copyBuf.end(), "07 5e 05 96 a4 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 16 1d ff ff ff ff 16")) {
		if (needPatch) *needPatch = false;
		return true;
	} else {
		if (needPatch) *needPatch = true;
	}
	if (checkOnly) return true;
	
/* Patch instructions:
Read 516/3ba-531/3c9 inclusive (28/16 bytes) as the bytecode for Param.SelectColor.
At 4f4/3a4 write 07 5e 05 96 a4 Param.SelectColor(with bStructWillBeModified=1) 16 1d ff ff ff ff 16 (40/28 bytes total, range 4f4/3a4-51b/3bf)
Fill 51c/3c0-533/3cb inclusive (24/12 bytes) with 0b (nop).
Read 71c/560-725/569 inclusive (10 bytes) as the bytecode for GetColorMax().
At 6de/53a write 07 45 07 99 a3 Param.SelectColor(with bStructWillBeModified=1) 16 GetColorMax() 16 0f Param.SelectColor(with bStructWillBeModified=1)
    1d ff ff ff ff (79/55 bytes total, range 6de/53a-72c/570)
Fill 72d/571-744/57c inclusive (24/12 bytes) with 0b (nop).
At 3cc for file insert 12 extra nops.
At 57d (the position specified here is the one from before the nop insertion above.
    It may have become greater by 12 bytes if you have done the insertion) for file insert 12 extra nops. */



	/* Originally the code segment looks like this. The script offsets on the left are provided in 123/122 format, and
	* the left number is the offset in the RAM representation of the bytecode (deserialized bytecode), while the right
	* number is the offset in the disk/.UPK representation of the bytecode (serialized bytecode).
	* The bytes after the ; are telling the actual bytes of the bytecode. They're also separated by /, and the left
	* side means the bytes from the RAM bytecode representation, while the right side means bytes from the disk/.UPK
	* bytecode representation.
	* The counts of bytes, whenever specified, are specified in decimal, while offsets and bytecodes are specified in hex.
	* Counts of bytes, if vary between RAM and disk representations, are also specified in pairs, separated by /.
	* 
4f4/3a4: SubtractSubtractInt ; int-- ; a6
4f5/3a5:   StructMember ; 35
             Property=SelectColor ; ?? ?? ?? ?? 00 00 00 00 / 00 1A 00 00
             Struct=CharaSelectPlayerParam ; ?? ?? ?? ?? 00 00 00 00 / 07 1A 00 00
             bMemberAccessRequiresStructCopy=0 ; 00
             bStructWillBeModified=1 ; 01
508/3b0:     LocalOutVariable ; 48
               Param ; ?? ?? ?? ?? 00 00 00 00 / 29 1B 00 00
511/3b5: EndFunctionParams ; 16
512/3b6: JumpIfNot ; 07
           -> 55e ; 5e 05  ; 55e specifies the file offset in RAM bytecode representation relative to the start of the whole script (it is same for both disk and RAM bytecodes)
515/3b9: Less_IntInt ; int < int ; 96
516/3ba:   StructMember ; 35
517/3bb:     Property=SelectColor ; ?? ?? ?? ?? 00 00 00 00 / 00 1A 00 00
51f/3bf:     Struct=CharaSelectPlayerParam ; ?? ?? ?? ?? 00 00 00 00 / 07 1A 00 00
527/3c3:     bMemberAccessRequiresStructCopy=0 ; 00
528/3c4:     bStructWillBeModified=0 ; 00
529/3c5:     LocalOutVariable ; 48
52a/3c6:       Param ; ?? ?? ?? ?? 00 00 00 00 / 29 1B 00 00
532/3ca:   IntZero ; 25
533/3cb: EndFunctionParams ; 16 */

	std::vector<unsigned char> patch;
	pushIntoUCharVector(patch, "07 5e 05 96 a4");
	patch.insert(patch.end(), ParamSelectColor_bStructWillBeModified_1.begin(), ParamSelectColor_bStructWillBeModified_1.end());
	pushIntoUCharVector(patch, "16 1d ff ff ff ff 16");
	/* This patch means:
4f4/3a4: JumpIfNot 5e5 (07 5e 05)  # 5e5 is the offset in RAM bytecode representation relative to the start of the whole script (it is same for both disk and RAM bytecodes)
4f7/3a7:     Less_IntInt (96)  # int < int
4f8/3a8: 	    SubtractSubtract_PreInt (a4)  # --int
4f9/3a9: 		    Param.SelectColor(with bStructWillBeModified=1) ; 28/16 bytes
515/3b9: 		EndFunctionParms (16)
516/3ba: 		IntConst (1d)
517/3bb: 		    -1 (ff ff ff ff)
51b/3bf: 	EndFunctionParms (16) */
	overwriteUCharVector(codeStart + 0x3a4, copyBuf.end(), patch);
	overwriteUCharVector(codeStart + 0x3c0, copyBuf.end(), 12, 0x0b);

	/* Original code segment:
6de/53a: AddAdd_Int ; int++ ; a5
6df/53b:   StructMember ; 35
6e0/53c:     Property=SelectColor ; ?? ?? ?? ?? 00 00 00 00 / 00 1A 00 00
6e8/540:     Struct=CharaSelectPlayerParam ; ?? ?? ?? ?? 00 00 00 00 / 07 1A 00 00
6f0/544:     bMemberAccessRequiresStructCopy=0 ; 00
6f1/545:     bStructWillBeModified=1 ; 01
6f2/546:     OutVariable Param ; 48 ?? ?? ?? ?? 00 00 00 00 / 48 29 1B 00 00
6fb/54b: EndFunctionParms ; 16
6fc/54c: JumpIfNot 745 ; 07 45 07
6ff/54f: GreaterEqual_IntInt ; 99
700/550:   StructMember ; 35
701/551:     Property=SelectColor ; ?? ?? ?? ?? 00 00 00 00 / 00 1A 00 00
709/555:     Struct=CharaSelectPlayerParam ; ?? ?? ?? ?? 00 00 00 00 / 07 1A 00 00
711/559:     bMemberAccessRequiresStructCopy=0 ; 00
712/55a:     bStructWillBeModified=0 ; 00
713/55b:     OutVariable Param ; 48 ?? ?? ?? ?? 00 00 00 00 / 48 29 1B 00 00
71c/560:   GetColorMax() ; 1B 28 0A 00 00 00 00 00 00 16
726/56a: EndFunctionParms ; 16
727/56b: Let ; 0f
728/56c:   StructMember ; 35
729/56d:     Property=SelectColor ; ?? ?? ?? ?? 00 00 00 00 / 00 1A 00 00
731/571:     Struct=CharaSelectPlayerParam ; ?? ?? ?? ?? 00 00 00 00 / 07 1A 00 00
739/575:     bMemberAccessRequiresStructCopy=0 ; 00
73a/576:     bStructWillBeModified=1 ; 01
73b/577:     OutVariable Param ; 48 ?? ?? ?? ?? 00 00 00 00 / 48 29 1B 00 00
744/57c:   IntZero ; 25
745/57d: */
	patch.clear();
	pushIntoUCharVector(patch, "07 45 07 99 a3");
	patch.insert(patch.end(), ParamSelectColor_bStructWillBeModified_1.begin(), ParamSelectColor_bStructWillBeModified_1.end());
	pushIntoUCharVector(patch, "16");
	patch.insert(patch.end(), GetColorMax.begin(), GetColorMax.end());
	pushIntoUCharVector(patch, "16 0f");
	patch.insert(patch.end(), ParamSelectColor_bStructWillBeModified_1.begin(), ParamSelectColor_bStructWillBeModified_1.end());
	pushIntoUCharVector(patch, "1d ff ff ff ff");
	/* This patch means:
6de/53a: JumpIfNot 745 (07 45 07)
6e1/53d:     GreaterEqual_IntInt (99) ; int >= int
6e2/53e: 	    AddAdd_PreInt (a3) ; ++int
6e3/53f: 	        Param.SelectColor(with bStructWillBeModified=1) ; 28/16 bytes
6ff/54f: 	    EndFunctionParms (16)
700/550: 	    GetColorMax() ; 10 bytes
70a/55a: 	EndFunctionParms (16)
70b/55b: Let (0f)
70c/55c:     Param.SelectColor(with bStructWillBeModified=1) ; 28/16 bytes
728/56c: 	IntConst (1d)
729/56d: 	    -1 (ff ff ff ff)
72d/571: */
	overwriteUCharVector(codeStart + 0x53a, copyBuf.end(), patch);
	overwriteUCharVector(codeStart + 0x571, copyBuf.end(), 12, 0x0b);

	copyBuf.insert(codeStart + 0x3cc, 12, 0x0b);
	codeStart = copyBuf.begin() + 0x30;
	copyBuf.insert(codeStart + 0x57d + 12, 12, 0x0b);
	// iterator codeStart is invalidated

	unsigned int oldSerializedLength = readFromUCharArray(copyBuf.begin() + 0x2c, copyBuf.end());
	overwriteUCharVector(copyBuf.begin() + 0x2c, copyBuf.end(), oldSerializedLength + 24);

	bytesRead = fwrite(copyBuf.data(), 1, copyBuf.size(), file);
	if (ferror(file)) {
		char errstr[1024] { '\0' };
		strerror_s(errstr, errno);
		lastError = L"Error writing to file: " + strToWStr(errstr);
		return false;
	}
	if (bytesRead != copyBuf.size()) {
		lastError = L"Wrote less bytes to the end of file than expected.";
		return false;
	}

	fseek(file, Player_UpdateCustomMenuExportsPtr->filePositionForSizeAndOffset, SEEK_SET);

	int newSerialSize = Player_UpdateCustomMenuExportsPtr->serialSize + 24;
	fwrite(&newSerialSize, 4, 1, file);
	fwrite(&newFilePositionForObject, 4, 1, file);
	fclose(file);
	closeFilesAtTheEnd.file = NULL;

	return true;
}
