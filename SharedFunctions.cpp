#include "SharedFunctions.h"

int findCharRevW(const wchar_t* buf, wchar_t c) {
    const wchar_t* ptr = buf;
    while (*ptr != '\0') {
        ++ptr;
    }
    while (ptr != buf) {
        --ptr;
        if (*ptr == c) return (int)(ptr - buf);
    }
    return -1;
}

int findLast(const std::wstring& str, wchar_t character) {
    return findCharRevW(str.c_str(), character);
}

bool fileExists(LPCWSTR path) {
	DWORD fileAttribs = GetFileAttributesW(path);
	if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	if ((fileAttribs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		return false;
	}
	return true;
}

bool fileOrFolderExists(LPCWSTR path) {
	DWORD fileAttribs = GetFileAttributesW(path);
	if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	return true;
}

bool fileExists(const std::wstring& path) {
    return fileExists(path.c_str());
}

bool fileOrFolderExists(const std::wstring& path) {
    return fileOrFolderExists(path.c_str());
}

bool folderExists(LPCWSTR path) {
	DWORD fileAttribs = GetFileAttributesW(path);
	if (fileAttribs == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	if ((fileAttribs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		return true;
	}
	return false;
}

bool folderExists(const std::wstring& path) {
    return folderExists(path.c_str());
}

std::wstring strToWStr(const std::string& str) {
	std::wstring result(str.size(), L'\0');
	auto it = result.begin();
	for (char c : str) {
		*it = (wchar_t)c;
		++it;
	}
	return result;
}

std::wstring getFileName(const std::wstring& path) {
    std::wstring result;
    int lastSlashPos = findLast(path, L'\\');
    if (lastSlashPos == -1) return path;
    result.insert(result.begin(), path.cbegin() + lastSlashPos + 1, path.cend());
    return result;
}

// Does not include trailing slash
std::wstring getParentDir(const std::wstring& path) {
    std::wstring result;
    int lastSlashPos = findLast(path, L'\\');
    if (lastSlashPos == -1) return result;
    result.insert(result.begin(), path.cbegin(), path.cbegin() + lastSlashPos);
    return result;
}

std::wstring generateUniqueBackupName(const std::wstring& path) {
    std::wstring fileName = getFileName(path);
    std::wstring parentDir = getParentDir(path);
	int dotPos = findLast(fileName, L'.');
	std::wstring nameExt;
	if (dotPos != -1) {
		nameExt = fileName.c_str() + dotPos;
		fileName.resize(dotPos);
	}
	std::wstring backupFilePathBase = parentDir + L'\\' + fileName;

    std::wstring backupFilePath = backupFilePathBase + L"_backup" + nameExt;
    int backupNameCounter = 1;
    while (fileOrFolderExists(backupFilePath)) {
        backupFilePath = backupFilePathBase + L"_backup" + std::to_wstring(backupNameCounter) + nameExt;
        ++backupNameCounter;
    }
	return backupFilePath;
}

bool pathsEqual(const std::vector<std::wstring>& pathA, const std::vector<std::wstring>& pathB, int* numberOfEqualNodes) {
	if (numberOfEqualNodes) *numberOfEqualNodes = 0;
	for (int i = 0; i < (int)pathA.size() && i < (int)pathB.size(); ++i) {
		const std::wstring& pathElementA = pathA[i];
		const std::wstring& pathElementB = pathB[i];
		if (pathElementA != pathElementB) {
			return false;
		}
		if (numberOfEqualNodes) ++*numberOfEqualNodes;
	}
	return !pathA.empty() && pathA.size() == pathB.size();
}
