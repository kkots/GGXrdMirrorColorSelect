#include "ExePatcher.h"

// The purpose of this file is to patch GuiltyGearXrd.exe to add instructions to it so that
// it stops rejecting the -1 (mirror) color and loops it around properly
#include "framework.h"
#include <iostream>
#include <vector>
#include "SharedFunctions.h"
#include "WinError.h"

using namespace std::literals;

// they banned strerror so I could do the exact same thing it did
static char errstr[1024] { '\0' };
static char sprintfbuf[1024] { '\0' };

// A class for simulating std::wcout << things.
// Prints everything into the debug console.
// Should I just extend wostream?.. Naah.
class OutputMachine {
public:
	OutputMachine& operator<<(char c) {
		char str[2] { '\0' };
		str[0] = c;
		OutputDebugStringA(str);
		return *this;
	}
	OutputMachine& operator<<(wchar_t c) {
		wchar_t wstr[2] { L'\0' };
		wstr[0] = c;
		OutputDebugStringW(wstr);
		return *this;
	}
	OutputMachine& operator<<(const wchar_t* wstr) {
		OutputDebugStringW(wstr);
		return *this;
	}
	OutputMachine& operator<<(const char* str) {
		OutputDebugStringA(str);
		return *this;
	}
	OutputMachine& operator<<(std::wostream& (*func)(std::wostream& in)) {
		if (func == std::endl<wchar_t, std::char_traits<wchar_t>>) {
			OutputDebugStringW(L"\n");
		}
		return *this;
	}
	OutputMachine& operator<<(std::ios_base& (*func)(std::ios_base& in)) {
		if (func == std::hex) {
			isHex = true;
		} else if (func == std::dec) {
			isHex = false;
		}
		return *this;
	}
	OutputMachine& operator<<(int i) {
		if (isHex) {
			sprintf_s(sprintfbuf, sizeof sprintfbuf, "%x", i);
			OutputDebugStringA(sprintfbuf);
		} else {
			OutputDebugStringW(std::to_wstring(i).c_str());
		}
		return *this;
	}
private:
	bool isHex = false;
} dout;

// byteSpecification is of the format "00 8f 1e ??". ?? means unknown byte.
// Converts a "00 8f 1e ??" string into two vectors:
// sig vector will contain bytes '00 8f 1e' for the first 3 bytes and 00 for every ?? byte.
// sig vector will be terminated with an extra 0 byte.
// mask vector will contain an 'x' character for every non-?? byte and a '?' character for every ?? byte.
// mask vector will be terminated with an extra 0 byte.
static void byteSpecificationToSigMask(const char* byteSpecification, std::vector<char>& sig, std::vector<char>& mask) {
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
				if (!nibblesUnknown) {
					sig.push_back(accumulatedNibbles & 0xff);
					mask.push_back('x');
					accumulatedNibbles >>= 8;
				} else {
					sig.push_back(0);
					mask.push_back('?');
				}
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
	sig.push_back('\0');
	mask.push_back('\0');
}

// 
// 

/// <summary>
/// A sigscan is an operation that looks for a pattern of bytes in a given region of binary data.
// The function returns the position of the start of the pattern within the region, relative to the start of the region,
// or -1 if the pattern is not found.
// The pattern is formed using -sig- and -mask- vectors and is described in byteSpecificationToSigMask function.
/// </summary>
/// <param name="start">The start of the searched region.</param>
/// <param name="end">The non-inclusive end of the searched region.</param>
/// <param name="sig">Signature, for ex. "\x80\x9f\xaa\x00\x00". Described in byteSpecificationToSigMask function.</param>
/// <param name="mask">Mask, for ex. "xxx??". Described in byteSpecificationToSigMask function.</param>
/// <returns>Position of the first matching pattern within the region, relative to start.
/// -1 if the pattern is not found.</returns>
static int sigscanSigMask(const char* start, const char* end, const char* sig, const char* mask) {
    const char* startPtr = start;
    const size_t maskLen = strlen(mask);
    const size_t seekLength = end - start - maskLen + 1;
    for (size_t seekCounter = seekLength; seekCounter != 0; --seekCounter) {
        const char* stringPtr = startPtr;

        const char* sigPtr = sig;
        for (const char* maskPtr = mask; true; ++maskPtr) {
            const char maskPtrChar = *maskPtr;
            if (maskPtrChar != '?') {
                if (maskPtrChar == '\0') return (int)(startPtr - start);
                if (*sigPtr != *stringPtr) break;
            }
            ++sigPtr;
            ++stringPtr;
        }
        ++startPtr;
    }
    return -1;
}

/// <summary>
/// A sigscan is an operation that looks for a pattern of bytes in a given region of binary data.
// The function returns the position of the start of the pattern relative to -fileStart-,
// or -1 if the pattern is not found.
// The pattern is formed using -sig- and -mask- vectors and is described in byteSpecificationToSigMask function.
/// </summary>
/// <param name="fileStart">The address of the start of the file data that was wholly read and loaded into memory.
/// Is not used to limit the searched region. Is only used to calculate the offset of the start of the pattern
/// relative to this -fileStart-.</param>
/// <param name="start">The start of the memory region to be searched. This start is expected to be no less than
/// -fileStart-.</param>
/// <param name="end">The non-inclusive end of the memory region to be searched.</param>
/// <param name="sig">Signature, for ex. "\x80\x9f\xaa\x00\x00". Described in byteSpecificationToSigMask function.</param>
/// <param name="mask">Mask, for ex. "xxx??". Described in byteSpecificationToSigMask function.</param>
/// <returns>Position of the first matching pattern within the in-memory (wholly read) file, relative to -fileStart-.
/// -1 if the pattern is not found.</returns>
static int sigscan(const char* fileStart, const char* start, const char* end, const char* sig, const char* mask) {
    int result = sigscanSigMask(start, end, sig, mask);
	if (result == -1) {
		return -1;
	} else {
		return result + (int)(start - fileStart);
	}
}

/// <summary>
/// A sigscan is an operation that looks for a pattern of bytes in a given region of binary data.
/// The function returns the position of the start of the pattern relative to -fileStart-,
/// or -1 if the pattern is not found.
/// The pattern is formed using byteSpecification.
/// </summary>
/// <param name="fileStart">The address of the start of the file data that was wholly read and loaded into memory.
/// Is not used to limit the searched region. Is only used to calculate the offset of the start of the pattern
/// relative to this -fileStart-.</param>
/// <param name="start">The start of the memory region to be searched. This start is expected to be no less than
/// -fileStart-.</param>
/// <param name="end">The non-inclusive end of the memory region to be searched.</param>
/// <param name="byteSpecification">byteSpecification is of the format "00 8f 1e ??". ?? means unknown byte, which can match any byte.</param>
/// <returns>Position of the first matching pattern within the in-memory (wholly read) file, relative to -fileStart-.
/// -1 if the pattern is not found.</returns>
static int sigscan(const char* fileStart, const char* start, const char* end, const char* byteSpecification) {
	
	std::vector<char> sig;
	std::vector<char> mask;
	byteSpecificationToSigMask(byteSpecification, sig, mask);

    int result = sigscanSigMask(start, end, sig.data(), mask.data());
	if (result == -1) {
		return -1;
	} else {
		return result + (int)(start - fileStart);
	}
}

static bool readWholeFile(FILE* file, std::vector<char>& wholeFile) {
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    wholeFile.resize(fileSize);
    char* wholeFilePtr = &wholeFile.front();
    size_t readBytesTotal = 0;
    while (true) {
        size_t sizeToRead = 1024;
        if (fileSize - readBytesTotal < 1024) sizeToRead = fileSize - readBytesTotal;
        if (sizeToRead == 0) break;
        size_t readBytes = fread(wholeFilePtr, 1, sizeToRead, file);
        if (readBytes != sizeToRead) {
            if (ferror(file)) {
                return false;
            }
            // assume feof
            break;
        }
        wholeFilePtr += 1024;
        readBytesTotal += 1024;
    }
    return true;
}

// Writes bytes into the file at the given -pos- without doing any shifting, meaning it overwrites any data
// at this region.
static bool writeBytesToFile(FILE* file, int pos, const char* bytesToWrite, size_t bytesToWriteCount) {
    fseek(file, pos, SEEK_SET);
    size_t writtenBytes = fwrite(bytesToWrite, 1, bytesToWriteCount, file);
    if (writtenBytes != bytesToWriteCount) {
        return false;
    }
    return true;
}

/// <summary>
/// Writes bytes into the file at the given -pos- without doing any shifting, meaning it overwrites any data
/// at this region.
/// </summary>
/// <param name="file">The file to write into.</param>
/// <param name="pos">The position to write at in the file.</param>
/// <param name="byteSpecification">byteSpecification is of the format "00 8f 1e". ?? bytes are interpreted as 00 here.</param>
/// <returns>true if no error encountered.</returns>
static bool writeBytesToFile(FILE* file, int pos, const char* byteSpecification) {
	std::vector<char> sig;
	std::vector<char> mask;
	byteSpecificationToSigMask(byteSpecification, sig, mask);

    fseek(file, pos, SEEK_SET);
    size_t writtenBytes = fwrite(sig.data(), 1, sig.size() - 1, file);
    if (writtenBytes != sig.size() - 1) {
        return false;
    }
    return true;
}

// A section of an .exe file, for example .text, .reloc section, etc.
struct Section {
    std::string name;

	// RVA. Virtual address offset relative to the virtual address start of the entire .exe.
	// So let's say the whole .exe starts at 0x400000 and RVA is 0x400.
	// That means the non-relative VA is 0x400000 + RVA = 0x400400.
	// Note that the .exe, although it does specify a base virtual address for itself on the disk,
	// may actually be loaded anywhere in the RAM once it's launched, and that RAM location will
	// become its base virtual address.
    uintptr_t relativeVirtualAddress = 0;

	// VA. Virtual address within the .exe.
	// A virtual address is the location of something within the .exe once it's loaded into memory.
	// An on-disk, file .exe is usually smaller than when it's loaded so it creates this distinction
	// between raw address and virtual address.
    uintptr_t virtualAddress = 0;

	// The size in terms of virtual address space.
    uintptr_t virtualSize = 0;

	// Actual position of the start of this section's data within the file.
    uintptr_t rawAddress = 0;

	// Size of this section's data on disk in the file.
    uintptr_t rawSize = 0;
};

// Reads info about sections such as .text, .reloc, etc.
static std::vector<Section> readSections(FILE* file) {

    std::vector<Section> result;

    uintptr_t peHeaderStart = 0;
    fseek(file, 0x3C, SEEK_SET);
    fread(&peHeaderStart, 4, 1, file);

    unsigned short numberOfSections = 0;
    fseek(file, (int)(peHeaderStart + 0x6), SEEK_SET);
    fread(&numberOfSections, 2, 1, file);

    uintptr_t optionalHeaderStart = peHeaderStart + 0x18;

    unsigned short optionalHeaderSize = 0;
    fseek(file, (int)(peHeaderStart + 0x14), SEEK_SET);
    fread(&optionalHeaderSize, 2, 1, file);

    uintptr_t imageBase = 0;
    fseek(file, (int)(peHeaderStart + 0x34), SEEK_SET);
    fread(&imageBase, 4, 1, file);

    uintptr_t sectionsStart = optionalHeaderStart + optionalHeaderSize;
    uintptr_t sectionStart = sectionsStart;
    for (size_t sectionCounter = numberOfSections; sectionCounter != 0; --sectionCounter) {
        Section newSection;
        fseek(file, (int)sectionStart, SEEK_SET);
        newSection.name.resize(8);
        fread(&newSection.name.front(), 1, 8, file);
        newSection.name.resize(strlen(newSection.name.c_str()));
        fread(&newSection.virtualSize, 4, 1, file);
        fread(&newSection.relativeVirtualAddress, 4, 1, file);
        newSection.virtualAddress = imageBase + newSection.relativeVirtualAddress;
        fread(&newSection.rawSize, 4, 1, file);
        fread(&newSection.rawAddress, 4, 1, file);
        result.push_back(newSection);
        sectionStart += 40;
    }

    return result;
}

bool performExePatching(const std::wstring& szFile, std::wstring& importantRemarks, HWND mainWindow, bool mayCreateBackup) {
    std::wstring fileName = getFileName(szFile);

	// Even if we're not modifying the file right away, we're still opening with the write access to check for the rights
    FILE* file = nullptr;
    if (_wfopen_s(&file, szFile.c_str(), L"r+b") || !file) {
		if (errno) {
			strerror_s(errstr, errno);
			MessageBoxW(mainWindow, (L"Failed to open " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		} else {
			MessageBoxW(mainWindow, (L"Failed to open " + fileName + L" file.").c_str(), L"Error", MB_OK);
		}
        if (file) {
            fclose(file);
        }
        return false;
    }

	class CloseFileOnExit {
	public:
		~CloseFileOnExit() {
			if (file) fclose(file);
		}
		FILE* file = NULL;
	} closeFileOnExit;

	closeFileOnExit.file = file;
	
    std::vector<Section> sections = readSections(file);
    if (sections.empty()) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": Failed to read sections.").c_str(), L"Error", MB_OK);
        return false;
    }

	Section* textSection = nullptr;
	for (Section& section : sections) {
		if (section.name == ".text" ) {
			textSection = &section;
			break;
		}
	}
	if (textSection == nullptr) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": .text section not found.").c_str(), L"Error", MB_OK);
        return false;
	}

    std::vector<char> wholeFile;
    if (!readWholeFile(file, wholeFile)) {
        strerror_s(errstr, sizeof errstr, errno);
		MessageBoxW(mainWindow, (L"Error reading file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}
    char* wholeFileBegin = &wholeFile.front();
	char* textSectionBegin = wholeFileBegin + textSection->rawAddress;
	char* textSectionEnd = wholeFileBegin + textSection->rawAddress + textSection->rawSize;
	
	if (mayCreateBackup) {
		// sig for ghidra: 83 3e 00 79 06 c7 06 18 00 00 00 ff 0e 90
		int alreadyPatchedCheck = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd,
			"83 3e 00 79 06 c7 06 18 00 00 00 ff 0e 90");
		if (alreadyPatchedCheck != -1) {
			importantRemarks += fileName + L" is already patched, so no patching or backing up took place.\n";
			return true;
		}
		
		fclose(file);
		closeFileOnExit.file = NULL;

		std::wstring backupFilePath = generateUniqueBackupName(szFile);
		dout << "Will use backup file path: " << backupFilePath.c_str() << std::endl;

		if (!CopyFileW(szFile.c_str(), backupFilePath.c_str(), false)) {
			WinError winErr;
			int response = MessageBoxW(mainWindow, (L"Failed to create a backup copy of " + fileName + L" ("
				+ backupFilePath + L"): " + winErr.getMessage() +
				L"Do you want to continue anyway? You won't be able to revert the file to the original. Press OK to agree.\n").c_str(),
				L"Warning", MB_OKCANCEL);
			if (response != IDOK) {
				return false;
			}
		} else {
			dout << "Backup copy created successfully.\n";
			importantRemarks += fileName + L" has been backed up to " + backupFilePath + L"\n";
		}
		return performExePatching(szFile, importantRemarks, mainWindow, false);
	}

    // sig for ghidra: 84 db 74 13 ff 0e 0f 89 43 ff ff ff c7 06 17 00 00 00 e9 38 ff ff ff
    int patchingPlace1 = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd,
        "84 db 74 13 ff 0e 0f 89 43 ff ff ff c7 06 17 00 00 00 e9 38 ff ff ff");
    if (patchingPlace1 == -1) {
		MessageBoxW(mainWindow, (L"Failed to patch " + fileName + L": Failed to find patching place 1.").c_str(), L"Error", MB_OK);
        return false;
    }
    patchingPlace1 += 4;
    dout << "Found patching place1: 0x" << std::hex << patchingPlace1 << std::dec << std::endl;
	char patch1[] = "83 3e 00 79 06 c7 06 18 00 00 00 ff 0e 90";
    if (!writeBytesToFile(file, patchingPlace1, patch1)) {
		strerror_s(errstr, errno);
		MessageBoxW(mainWindow, (L"Error writing patch1 to " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}

    // sig for ghidra: 84 db 74 15 ff 0e 0f 89 43 ff ff ff 8b c6 c7 00 17 00 00 00 e9 36 ff ff ff
    int patchingPlace2 = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd,
        "84 db 74 15 ff 0e 0f 89 43 ff ff ff 8b c6 c7 00 17 00 00 00 e9 36 ff ff ff");
    if (patchingPlace2 == -1) {
		MessageBoxW(mainWindow, (L"Failed to patch " + fileName + L": Failed to find patching place 2.").c_str(), L"Error", MB_OK);
        return false;
    }
    patchingPlace2 += 4;
    dout << "Found patching place2: 0x" << std::hex << patchingPlace2 << std::dec << std::endl;
	char patch2[] = "83 3e 00 79 06 c7 06 18 00 00 00 ff 0e 0f 1f 00";
    if (!writeBytesToFile(file, patchingPlace2, patch2)) {
		strerror_s(errstr, errno);
		MessageBoxW(mainWindow, (L"Error writing patch2 to " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}

    // sig for ghidra: 84 db 74 18 ff 06 83 3e 18 0f 8c 40 ff ff ff 8b d6 c7 02 00 00 00 00 e9 33 ff ff ff
    int patchingPlace3 = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd,
        "84 db 74 18 ff 06 83 3e 18 0f 8c 40 ff ff ff 8b d6 c7 02 00 00 00 00 e9 33 ff ff ff");
    if (patchingPlace3 == -1) {
		MessageBoxW(mainWindow, (L"Failed to patch " + fileName + L": Failed to find patching place 3.").c_str(), L"Error", MB_OK);
        return false;
    }
    patchingPlace3 += 0x13;
    dout << "Found patching place3: 0x" << std::hex << patchingPlace3 << std::dec << std::endl;
	char patch3[] = "ff ff ff ff";
    if (!writeBytesToFile(file, patchingPlace3, patch3)) {
		strerror_s(errstr, errno);
		MessageBoxW(mainWindow, (L"Error writing patch3 to " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}

    // sig for ghidra: 84 db 74 18 ff 06 83 3e 18 0f 8c 34 ff ff ff 8b d6 c7 02 00 00 00 00 e9 27 ff ff ff
    int patchingPlace4 = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd,
        "84 db 74 18 ff 06 83 3e 18 0f 8c 34 ff ff ff 8b d6 c7 02 00 00 00 00 e9 27 ff ff ff");
    if (patchingPlace4 == -1) {
		MessageBoxW(mainWindow, (L"Failed to patch " + fileName + L": Failed to find patching place 4.").c_str(), L"Error", MB_OK);
        return false;
    }
    patchingPlace4 += 0x13;
    dout << "Found patching place4: 0x" << std::hex << patchingPlace4 << std::dec << std::endl;
	char patch4[] = "ff ff ff ff";
    if (!writeBytesToFile(file, patchingPlace4, patch4)) {
		strerror_s(errstr, errno);
		MessageBoxW(mainWindow, (L"Error writing patch4 to " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}

    // sig for ghidra: cc cc cc cc cc cc cc 53 8b 5c 24 0c 85 db 78 32 8b 44 24 08 56 57 50 e8 ?? ?? ?? ?? 8b f0 8b fa 83 c4 04 b8 01 00 00 00 33 d2 8b cb e8 ?? ?? ?? ?? 23 f0 23 fa 0b f7 5f 5e 74 07 b8 01 00 00 00 5b c3 33 c0 5b c3
    int patchingPlace5 = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd,
        "cc cc cc cc cc cc cc 53 8b 5c 24 0c 85 db 78 32 8b 44 24 08 56 57 50 e8 ?? ?? ?? ?? 8b f0 8b fa 83 c4 04 b8 01 00 00 00 33 d2 8b cb e8 ?? ?? ?? ?? 23 f0 23 fa 0b f7 5f 5e 74 07 b8 01 00 00 00 5b c3 33 c0 5b c3");
    if (patchingPlace5 == -1) {
		MessageBoxW(mainWindow, (L"Failed to patch " + fileName + L": Failed to find patching place 4.").c_str(), L"Error", MB_OK);
        return false;
    }
    dout << "Found patching place5: 0x" << std::hex << patchingPlace5 << std::dec << std::endl;
	char patch5[] = "89 d8 40 74 36 eb 07";
    if (!writeBytesToFile(file, patchingPlace5, patch5)) {
		strerror_s(errstr, errno);
		MessageBoxW(mainWindow, (L"Error writing patch5 to " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}
	char patch6[] = "eb f2";
    if (!writeBytesToFile(file, patchingPlace5 + 7 + 5, patch6)) {
		strerror_s(errstr, errno);
		MessageBoxW(mainWindow, (L"Error writing patch6 to " + fileName + L" file: " + strToWStr(errstr)).c_str(), L"Error", MB_OK);
		return false;
	}

	importantRemarks += L"Patched GuiltyGearXrd.exe.\n";
    dout << "Patched successfully\n";

    fclose(file);
	closeFileOnExit.file = NULL;
	return true;
}
