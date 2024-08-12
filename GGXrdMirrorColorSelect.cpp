// GGXrdMirrorColorSelect.cpp : Defines the entry point for the application.
//

#ifndef UNICODE
#define UNICODE
#endif
#include "framework.h"
#include "GGXrdMirrorColorSelect.h"
#include <windowsx.h>
#include <CommCtrl.h>
#include <Shlobj.h> // for CLSID_FileOpenDialog
#include <atlbase.h> // for CComPtr
#include <vector>
#include <algorithm>
#include <string>
#include <Psapi.h>
#include <VersionHelpers.h>
#include "WinError.h"
#include "VdfReader.h"
#include <unordered_map>
#include "SharedFunctions.h"
#include "ExePatcher.h"
#include "UpkPatcher.h"

using namespace std::literals;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
std::vector<HWND> whiteBgLabels;
HBRUSH hbrBkgnd = NULL;
HFONT font = NULL;
HWND hWndButton = NULL;
HWND mainWindow = NULL;

// The patch of the GuiltyGearXrd.exe file that was obtained when the GuiltyGearXrd.exe process was seen working.
std::wstring ggProcessModulePath;
std::wstring steamLibraryPath;

// The Y position at which the next text row using addTextRow must be created.
int nextTextRowY = 5;

// Used to restart adding text rows from some fixed Y position.
int nextTextRowYOriginal = 5;

// These variables are used for text measurement when creating controls, to determine their sizes.
HDC hdc = NULL;
HGDIOBJ oldObj = NULL;

// Specifies text color for each static control.
std::unordered_map<HWND, COLORREF> controlColorMap;

// Stores the result of the GetTempPath() call.
wchar_t tempPath[MAX_PATH] { L'\0' };

// The text rows that have been added beneath the "Patch GuiltyGear" button.
// They must be cleared on each button press so that the new output can be displayed
// in that same place.
std::vector<HWND> rowsToRemove;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK setFontOnChildWindow(HWND hwnd, LPARAM lParam);
void handlePatchButton();
HANDLE findOpenGgProcess(bool* foundButFailedToOpen);
void fillGgProcessModulePath(HANDLE ggProcess);
bool validateInstallPath(std::wstring& path);
#define stringAndItsLength(str) str, (int)wcslen(str)
void beginInsertRows();
void finishInsertRows();
HWND addTextRow(const wchar_t* txt);
void printRemarks(std::vector<std::wstring>& remarks);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_GGXRDMIRRORCOLORSELECT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_GGXRDMIRRORCOLORSELECT));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_GGXRDMIRRORCOLORSELECT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_GGXRDMIRRORCOLORSELECT);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	mainWindow = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!mainWindow)
	{
		return FALSE;
	}

	ShowWindow(mainWindow, nCmdShow);
	UpdateWindow(mainWindow);

	hbrBkgnd = GetSysColorBrush(COLOR_WINDOW);
	TEXTMETRIC textMetric { 0 };
	NONCLIENTMETRICSW nonClientMetrics { 0 };
	nonClientMetrics.cbSize = sizeof(NONCLIENTMETRICSW);
	SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &nonClientMetrics, NULL);
	font = CreateFontIndirectW(&nonClientMetrics.lfCaptionFont);

	
	beginInsertRows();

	addTextRow(L"App for patching Binaries\\Win32\\GuitlyGearXrd.exe and REDGame\\CookedPCConsole\\REDGame.upk,");

	addTextRow(L"so that you could select the mirror color in Guilty Gear Xrd -REVELATOR- Rev 2 version 2211 (works as of 2024 August 11).");
	
	addTextRow(L"Backups of GuitlyGearXrd.exe and REDGame.upk will be created,");
	
	addTextRow(L"so in case the game stops working after patching, substitute the backup copies of GuitlyGearXrd.exe and REDGame.upk");

	addTextRow(L"for the current ones. All these files should be located in the game's installation directory in corresponding subdirectories.");
	
	const wchar_t* title = L"Patch GuiltyGear";
	SIZE textSz{0};
	GetTextExtentPoint32W(hdc, stringAndItsLength(title), &textSz);
	hWndButton = CreateWindowW(WC_BUTTONW, title,
				WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
				5, nextTextRowY, textSz.cx + 20, textSz.cy + 8, mainWindow, NULL, hInst,
				NULL);
	nextTextRowY += textSz.cy + 8;
	nextTextRowYOriginal = nextTextRowY;

	finishInsertRows();
	EnumChildWindows(mainWindow, setFontOnChildWindow, (LPARAM)font);
	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_CTLCOLORSTATIC: {
		// Provides background and text colors for static (text) controls.
		auto found = controlColorMap.find((HWND)lParam);
		if (found != controlColorMap.end()) {
			HDC hdcStatic = (HDC) wParam;
			SetTextColor(hdcStatic, found->second);
		}
		if (std::find(whiteBgLabels.begin(), whiteBgLabels.end(), (HWND)lParam) != whiteBgLabels.end()) {
			return (INT_PTR)hbrBkgnd;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;
    case WM_COMMAND:
		// A button click
        {
			if ((HWND)lParam == hWndButton) {
				// The "Patch GuiltyGear" button
				handlePatchButton();
				break;
			}
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // Not drawing anything here yet
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL CALLBACK setFontOnChildWindow(HWND hwnd, LPARAM lParam) {
	SendMessageW(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
	return TRUE;
}

// Finds if GuiltyGearXrd.exe is currently open and returns a handle to its process.
// -foundButFailedToOpen- may be used to learn that the process was found, but a handle
// to it could not be retrieved.
HANDLE findOpenGgProcess(bool* foundButFailedToOpen) {
    if (foundButFailedToOpen) *foundButFailedToOpen = false;
    // this method was chosen because it's much faster than enumerating all windows or all processes and checking their names
    const HWND foundGgWindow = FindWindowW(L"LaunchUnrealUWindowsClient", L"Guilty Gear Xrd -REVELATOR-");
    if (!foundGgWindow) return NULL;
    DWORD windsProcId = 0;
    GetWindowThreadProcessId(foundGgWindow, &windsProcId);
    HANDLE windsProcHandle = OpenProcess(
		// because GetModuleFileNameExW MSDN page says just this right is enough for Windows10+
		IsWindows10OrGreater() ? PROCESS_QUERY_LIMITED_INFORMATION : PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
		FALSE, windsProcId);
    if (!windsProcHandle && foundButFailedToOpen) {
        *foundButFailedToOpen = true;
    }
    return windsProcHandle;
}

// Uses Steam config files or other methods to discover the directory where Guilty Gear Xrd is installed.
// Returns true if a directory was found. The directory still needs to be validated.
bool findGgInstallPath(std::wstring& path) {
	
	if (!ggProcessModulePath.empty()) {
		for (int i = 0; i < 3; ++i) {
			int pos = findCharRevW(ggProcessModulePath.c_str(), L'\\');
			if (pos < 1) {
				break;
			}
			if (pos > 0 && (ggProcessModulePath[pos - 1] == L'\\' || ggProcessModulePath[pos - 1] == L':')) {
				break;
			}
			ggProcessModulePath.resize(pos);
		}
		if (!ggProcessModulePath.empty()) {
			path = ggProcessModulePath;
			return true;
		}
		return false;
	}

	class CloseStuffOnReturn {
	public:
		HKEY hKey = NULL;
		~CloseStuffOnReturn() {
			if (hKey) RegCloseKey(hKey);
		}
	} closeStuffOnReturn;

	HKEY steamReg = NULL;
	LSTATUS lStatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", NULL, KEY_READ, &steamReg);
	if (lStatus != ERROR_SUCCESS) {
		WinError err;
		OutputDebugStringW(L"RegOpenKeyExW failed: ");
		OutputDebugStringW(err.getMessage());
		OutputDebugStringW(L"\n");
		return false;
	}
	closeStuffOnReturn.hKey = steamReg;

	wchar_t gimmePath[MAX_PATH] { L'0' };
	DWORD gimmePathSize = sizeof gimmePath;
	DWORD dataType = 0;
	lStatus = RegQueryValueExW(steamReg, L"InstallPath", NULL, &dataType, (BYTE*)gimmePath, &gimmePathSize);
	if (lStatus != ERROR_SUCCESS) {
		WinError err;
		OutputDebugStringW(L"RegQueryValueExW failed: ");
		OutputDebugStringW(err.getMessage());
		OutputDebugStringW(L"\n");
		return false;
	}
	RegCloseKey(steamReg);
	closeStuffOnReturn.hKey = NULL;
	steamReg = NULL;

	if (dataType != REG_SZ) {
		OutputDebugStringW(L"RegQueryValueExW returned not a string\n");
		return false;
	}
	if (gimmePathSize == MAX_PATH && gimmePath[MAX_PATH - 1] != L'\0' || gimmePathSize >= MAX_PATH) {
		OutputDebugStringW(L"RegQueryValueExW returned a string that's too big probably\n");
		return false;
	}
	if (gimmePathSize == 0) {
		OutputDebugStringW(L"RegQueryValueExW returned an empty string without even a null character\n");
		return false;
	}
	gimmePath[gimmePathSize - 1] = L'\0';
	if (gimmePathSize == 1) {
		OutputDebugStringW(L"RegQueryValueExW returned an empty string\n");
		return false;
	}
	std::wstring steamPath = gimmePath;
	if (steamPath.empty() || steamPath[steamPath.size() - 1] != L'\\') {
		steamPath += L'\\';
	}
	steamPath += L"config\\libraryfolders.vdf";
	VdfReader reader(steamPath);
	int i = 0;
	bool containsI;
	do {
		std::wstring iAsString = std::to_wstring(i);
		containsI = reader.hasKey(L"libraryfolders", iAsString);
		if (containsI) {
			std::wstring appId = L"520440";
			if (reader.hasKey(L"libraryfolders/" + iAsString + L"/apps", appId)) {
				reader.getValue(L"libraryfolders/" + iAsString + L"/path", steamLibraryPath);
				path = steamLibraryPath;
				if (path.empty() || path[path.size() - 1] != L'\\') {
					path += L'\\';
				}
				path += L"steamapps\\common\\GUILTY GEAR Xrd -REVELATOR-";
				return true;
			}
		}
		++i;
	} while (containsI);
	return false;
}

// Asks the user to select a directory through a dialog.
bool selectFolder(std::wstring& path) {
    wchar_t pszBuf[MAX_PATH] = { '\0' };
    CComPtr<IFileDialog> fileDialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_IFileDialog, (LPVOID*)&fileDialog))) return false;
    if (!fileDialog) return false;
    DWORD dwFlags;
    if (FAILED(fileDialog->GetOptions(&dwFlags))) return false;
    if (FAILED(fileDialog->SetOptions(dwFlags | FOS_PICKFOLDERS))) return false;
    if (!path.empty()) {
        CComPtr<IShellItem> shellItem;
        if (SUCCEEDED(SHCreateItemFromParsingName(path.c_str(), NULL, IID_IShellItem, (void**)&shellItem))) {
            fileDialog->SetFolder(shellItem);
        }
    }
    if (fileDialog->Show(mainWindow) != S_OK) return false;
    IShellItem* psiResult = nullptr;
    if (FAILED(fileDialog->GetResult(&psiResult))) return false;
    SFGAOF attribs{ 0 };
    if (FAILED(psiResult->GetAttributes(SFGAO_FILESYSTEM, &attribs))) return false;
    if ((attribs & SFGAO_FILESYSTEM) == 0) {
        MessageBoxW(mainWindow, L"The selected file is not a file system file.", L"Error", MB_OK);
        return false;
    }
    LPWSTR pszName = nullptr;
    if (FAILED(psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszName))) return false;
    path = pszName;
    CoTaskMemFree(pszName);
    return true;
}

// Obtains the path of the GuiltyGearXrd.exe executable via its process and stores it into -ggProcessModulePath-.
void fillGgProcessModulePath(HANDLE ggProcess) {
	// sneaky or convenient? User is completely unaware of this feature, UI is not communicating it in any way
	ggProcessModulePath.resize(MAX_PATH - 1, L'\0');
	if (GetModuleFileNameExW(ggProcess, NULL, &ggProcessModulePath.front(), MAX_PATH) == 0) {
		WinError err;
		OutputDebugStringW(L"GetModuleFileNameExW failed: ");
		OutputDebugStringW(err.getMessage());
		OutputDebugStringW(L"\n");
		ggProcessModulePath.clear();
		return;
	}
	if (ggProcessModulePath.empty() || ggProcessModulePath[ggProcessModulePath.size() - 1] != L'\0') {
		OutputDebugStringW(L"fillGgProcessModulePath: ggProcessModulePath not null-terminated\n");
		ggProcessModulePath.clear();
		return;
	}
	ggProcessModulePath.resize(wcslen(ggProcessModulePath.c_str()));
}

// Checks if the provided installation path is valid.
bool validateInstallPath(std::wstring& path) {
	std::wstring pathFixed = path;
	if (pathFixed.empty() || pathFixed[pathFixed.size() - 1] != L'\\') {
		pathFixed += L'\\';
	}
	std::wstring subPath;

	subPath = pathFixed + L"Binaries\\Win32\\GuiltyGearXrd.exe";
	if (!fileExists(subPath.c_str())) {
		return false;
	}
	
	subPath = pathFixed + L"REDGame\\CookedPCConsole\\REDGame.upk";
	if (!fileExists(subPath.c_str())) {
		return false;
	}

	return true;

}

// Calls this once before calling addTextRow one or more times
void beginInsertRows() {
	hdc = GetDC(mainWindow);
	oldObj = SelectObject(hdc, (HGDIOBJ)font);
}

// Calls this once after calling addTextRow one or more times
void finishInsertRows() {
	SelectObject(hdc, oldObj);
	ReleaseDC(mainWindow, hdc);
	hdc = NULL;
	oldObj = NULL;
}

// One or more calls to this method must be wrapped by a single call to beginInsertRows()
// and a single calls to finishInsertRows().
// Adds a row of text at -nextTextRowY-.
HWND addTextRow(const wchar_t* txt) {
	SIZE textSz{0};
	GetTextExtentPoint32W(hdc, stringAndItsLength(txt), &textSz);
	HWND newWindow = CreateWindowW(WC_STATICW, txt,
			WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
			5, nextTextRowY, textSz.cx, textSz.cy, mainWindow, NULL, hInst,
			NULL);
	whiteBgLabels.push_back(newWindow);
	nextTextRowY += textSz.cy + 5;
	return newWindow;
}

// Breaks up a string into multiple by -c- char. The new strings do not include the delimiter.
std::vector<std::wstring> split(const std::wstring& str, wchar_t c) {
	std::vector<std::wstring> result;
	const wchar_t* strStart = &str.front();
	const wchar_t* strEnd = strStart + str.size();
	const wchar_t* prevPtr = strStart;
	const wchar_t* ptr = strStart;
	while (*ptr != L'\0') {
		if (*ptr == c) {
			if (ptr > prevPtr) {
				result.emplace_back(prevPtr, ptr - prevPtr);
			}
			prevPtr = ptr + 1;
		}
		++ptr;
	}
	if (prevPtr < strEnd) {
		result.emplace_back(prevPtr, strEnd - prevPtr);
	}
	return result;
}

// Creates one or more text rows for each element in -remarks-.
// It depends on whether the text row's character count is below some fixed predetermined count.
// If it's greater the row gets split into multiple.
// All rows are colored blue.
void printRemarks(std::vector<std::wstring>& remarks) {
	for (std::wstring& line : remarks) {
		beginInsertRows();
		while (!line.empty()) {
			std::wstring subline = line;
			const int charsMax = 150;
			if (line.size() > charsMax) {
				subline.resize(charsMax);
				line = line.substr(charsMax);
			} else {
				line.clear();
			}
			HWND newWindow = addTextRow(subline.c_str());
			rowsToRemove.push_back(newWindow);
			setFontOnChildWindow(newWindow, NULL);
			controlColorMap.insert({newWindow, RGB(0, 0, 255)});
			UpdateWindow(newWindow);
		}
		finishInsertRows();
	}
}

// The "Patch GuiltyGear" button handler.
void handlePatchButton() {
	// Delete messages from the previous run of the handlePatchButton()
	for (HWND hwnd : rowsToRemove) {
		DestroyWindow(hwnd);
		controlColorMap.erase(hwnd);
		for (auto it = whiteBgLabels.begin(); it != whiteBgLabels.end(); ++it) {
			if (*it == hwnd) {
				whiteBgLabels.erase(it);
				break;
			}
		}
	}
	rowsToRemove.clear();
	nextTextRowY = nextTextRowYOriginal;

	// GuiltyGearXrd.exe must not be running during the patching process.
	bool foundButFailedToOpen = false;
	HANDLE ggProcess = findOpenGgProcess(&foundButFailedToOpen);
	if (ggProcess || foundButFailedToOpen) {
		if (ggProcess) {
			// We can use the path of GuiltyGearXrd.exe as a backup mechanism for determining the installation directory of the game.
			fillGgProcessModulePath(ggProcess);
			CloseHandle(ggProcess);
			ggProcess = NULL;
		}
		MessageBoxW(mainWindow, L"Guilty Gear Xrd is currently open. Please close it and try again.", L"Error", MB_OK);
		return;
	}
	if (ggProcess) {
		CloseHandle(ggProcess);
		ggProcess = NULL;
	}
	
	// Find where the game is installed.
	std::wstring installPath;
	if (!(findGgInstallPath(installPath) && !installPath.empty() && validateInstallPath(installPath))) {
		installPath.clear();
	}
	if (installPath.empty()) {
		int response = MessageBoxW(mainWindow, L"Please select the Guilty Gear Xrd installation directory.", L"Message", MB_OKCANCEL);
		if (response != IDOK) {
			return;
		}
		// Ask the user to provide the installation directory manually if we failed to find it.
		if (!selectFolder(installPath)) {
			installPath.clear();
		}
		if (!validateInstallPath(installPath)) {
			MessageBoxW(mainWindow, L"Could not locate REDGame\\CookedPCConsole\\REDGame.upk and Binaries\\Win32\\GuiltyGearXrd.exe in the"
				" selected folder. Please select another folder and try again.", L"Error", MB_OK);
			installPath.clear();
		}
	}
	if (installPath.empty()) return;
	if (installPath[installPath.size() - 1] != L'\\') {
		installPath += L'\\';
	}

	// Patch GuiltyGearXrd.exe.
	std::wstring remarks;
	if (!performExePatching(installPath + L"Binaries\\Win32\\GuiltyGearXrd.exe", remarks, mainWindow)) {
		return;
	}
	std::vector<std::wstring> remarksLines = split(remarks, L'\n');
	printRemarks(remarksLines);
	remarksLines.clear();
	
	class TempFileDeleter {
	public:
		~TempFileDeleter() {
			for (std::wstring& p : tempFile) {
				DeleteFileW(p.c_str());
			}
			for (std::wstring& p : tempFolder) {
				RemoveDirectoryW(p.c_str());
			}
			if (remarksToPrint) printRemarks(*remarksToPrint);
		}
		void unwatch(std::wstring path) {
			unwatch(tempFile, path);
			unwatch(tempFolder, path);
		}
		void unwatch(std::vector<std::wstring>& vec, std::wstring path) {
			for (auto it = vec.begin(); it != vec.end(); ++it) {
				if (*it == path) {
					vec.erase(it);
					break;
				}
			}
		}
		std::vector<std::wstring> tempFile;
		std::vector<std::wstring> tempFolder;
		std::vector<std::wstring>* remarksToPrint = nullptr;
	} tempFileDeleter;

	std::wstring upkOriginal = installPath + L"REDGame\\CookedPCConsole\\REDGame.upk";
	if (!isUpk(upkOriginal)) {
		// If the file is not a .UPK file, that means it's encrypted, so we must decrypted and decompress it.
		// It also means that it's definitely not patched.
		if (!getLastUpkError().empty()) {
			MessageBoxW(mainWindow, (L"Error reading " + upkOriginal + L": " + getLastUpkError()).c_str(), L"Error", MB_OK);
			return;
		}
		if (tempPath[0] == L'\0') {
			if (GetTempPathW(MAX_PATH, tempPath) == 0) {
				WinError winErr;
				MessageBoxW(mainWindow, (L"Could not get TEMP directory: "s + winErr.getMessage()).c_str(), L"Error", MB_OK);
				return;
			}
		}

		// Create a temporary directory.
		wchar_t tempFolder[MAX_PATH] { '\0' };
		if (GetTempFileNameW(tempPath, L"GGXrdMirrorColorSelect", 0, tempFolder) == 0) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Could not create a temporary file: "s + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}
		if (!DeleteFileW(tempFolder)) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to delete "s + tempFolder + L": " + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}
		if (!CreateDirectoryW(tempFolder, NULL)) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to create a temporary directory ('"s + tempFolder + L"'): " + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}

		tempFileDeleter.tempFolder.push_back(tempFolder);

		// Copy REDGame.upk into the temporary directory.
		std::wstring tempUpkPath = tempFolder;
		tempUpkPath += L"\\REDGame.upk";
		if (!CopyFileW(upkOriginal.c_str(), tempUpkPath.c_str(), true)) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to copy REDGame.upk to the temporary directory ('"s + tempFolder + L"'): " + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}

		tempFileDeleter.tempFile.push_back(tempUpkPath);

		// Decrypt REDGame.upk using the decrypted and save it as REDGame.upk.dec in that same directory.
		STARTUPINFOW startupInfo{0};
		startupInfo.cb = sizeof(STARTUPINFOW);
		PROCESS_INFORMATION processInfo{0};
		std::wstring commandLineStr = L"3rdparty\\GGXrdRevelator.exe -revel \"" + tempUpkPath + L"\"";
		size_t commandLineSize = (commandLineStr.size() + 1) * sizeof(wchar_t);
		wchar_t* commandLine = (wchar_t*)malloc(commandLineSize);
		if (!commandLine) {
			MessageBoxW(mainWindow, L"Failed to allocate memory.", L"Error", MB_OK);
			return;
		}
		memcpy(commandLine, commandLineStr.c_str(), commandLineSize);
		if (!CreateProcessW(L"3rdparty\\GGXrdRevelator.exe", commandLine, NULL, NULL, false, NULL, NULL, tempFolder, &startupInfo, &processInfo)) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to launch 3rdparty\\GGXrdRevelator.exe: "s + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		if (startupInfo.hStdInput) CloseHandle(startupInfo.hStdInput);
		if (startupInfo.hStdOutput) CloseHandle(startupInfo.hStdOutput);
		if (startupInfo.hStdError) CloseHandle(startupInfo.hStdError);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		free(commandLine);

		tempUpkPath += L".dec";

		if (!fileExists(tempUpkPath)) {
			WinError winErr;
			MessageBoxW(mainWindow, L"GGXrdRevelator.exe tool failed to decrypt REDGame.upk.", L"Error", MB_OK);
			return;
		}

		tempFileDeleter.tempFile.push_back(tempUpkPath);

		// Decompress REDGame.upk.dec using the decompression tool.
		// The decompressed file REDGame.upk.dec will appear in the 'unpacked' directory within the temporary directory.
		commandLineStr = L"3rdparty\\decompress.exe " + tempUpkPath;
		commandLineSize = (commandLineStr.size() + 1) * sizeof(wchar_t);
		commandLine = (wchar_t*)malloc(commandLineSize);
		if (!commandLine) {
			MessageBoxW(mainWindow, L"Failed to allocate memory.", L"Error", MB_OK);
			return;
		}
		memcpy(commandLine, commandLineStr.c_str(), commandLineSize);
		if (!CreateProcessW(L"3rdparty\\decompress.exe", commandLine, NULL, NULL, false, NULL, NULL, tempFolder, &startupInfo, &processInfo)) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to launch 3rdparty\\decompress.exe: "s + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		if (startupInfo.hStdInput) CloseHandle(startupInfo.hStdInput);
		if (startupInfo.hStdOutput) CloseHandle(startupInfo.hStdOutput);
		if (startupInfo.hStdError) CloseHandle(startupInfo.hStdError);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
		free(commandLine);

		bool folderAndFileOk = false;
		if (folderExists(tempFolder + L"\\unpacked"s)) {
			tempFileDeleter.tempFolder.insert(tempFileDeleter.tempFolder.begin(), tempFolder + L"\\unpacked"s);

			tempUpkPath = tempFolder + L"\\unpacked\\REDGame.upk.dec"s;
			if (fileExists(tempUpkPath)) {
				folderAndFileOk = true;
				tempFileDeleter.tempFile.push_back(tempFolder + L"\\unpacked\\REDGame.upk.dec"s);
			}
		}
		if (!folderAndFileOk) {
			WinError winErr;
			MessageBoxW(mainWindow, L"decompress.exe failed to unpack REDGame.upk", L"Error", MB_OK);
			return;
		}

		// Patch the now decrypted and decompressed UPK in-place (no extra copies).
		if (!performUpkPatching(tempUpkPath)) {
			MessageBoxW(mainWindow, (L"Failed to patch REDGame.upk: " + getLastUpkError()).c_str(), L"Error", MB_OK);
			return;
		}

		// Backup the original REDGame.upk, the one that is in the game's directory.
		std::wstring backupName = generateUniqueBackupName(upkOriginal);
		if (!MoveFileW(upkOriginal.c_str(), backupName.c_str())) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to rename " + upkOriginal + L" to " + backupName + L": " + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}

		remarksLines.push_back(L"Created backup copy of REDGame.upk at " + backupName);
		tempFileDeleter.remarksToPrint = &remarksLines;

		// Place the new, patched REDGame.upk.dec, into the game's directory, and rename it to REDGame.upk.
		if (!MoveFileW(tempUpkPath.c_str(), upkOriginal.c_str())) {
			WinError winErr;
			MessageBoxW(mainWindow, (L"Failed to rename " + tempUpkPath + L" to " + upkOriginal + L": " + winErr.getMessage()).c_str(), L"Error", MB_OK);
			return;
		}

		tempFileDeleter.unwatch(tempUpkPath);

		remarksLines.push_back(L"Patched REDGame.upk.");

	} else {
		
		// If the file is a .UPK, that means it is not encrypted, and we will assume that it is not compressed either.
		// That means it could potentially be already patched, so we need to check that.

		// Do not patch the UPK, simply check if it needs to be patched.
		bool needPatch = false;
		if (!performUpkPatching(upkOriginal, true, &needPatch)) {
			MessageBoxW(mainWindow, (L"Failed to read REDGame.upk: " + getLastUpkError()).c_str(), L"Error", MB_OK);
			return;
		}
		if (!needPatch) {
			remarksLines.push_back(L"REDGame.upk is already patched, so no backing up or patching took place.");
		} else {
			// The UPK needs to be patched.
			// Create a backup copy of it.
			std::wstring backupName = generateUniqueBackupName(upkOriginal);
			if (!CopyFileW(upkOriginal.c_str(), backupName.c_str(), true)) {
				WinError winErr;
				MessageBoxW(mainWindow, (L"Failed to copy " + upkOriginal + L" to " + backupName + L": " + winErr.getMessage()).c_str(), L"Error", MB_OK);
				return;
			}
			
			remarksLines.push_back(L"Created backup copy of REDGame.upk at " + backupName);

			// Patch the UPK file in-place.
			if (!performUpkPatching(upkOriginal)) {
				MessageBoxW(mainWindow, (L"Failed to patch REDGame.upk: " + getLastUpkError()).c_str(), L"Error", MB_OK);
				return;
			}
			
			remarksLines.push_back(L"Patched REDGame.upk.");

		}
	}
	
	printRemarks(remarksLines);
	remarksLines.clear();
	tempFileDeleter.remarksToPrint = nullptr;

	MessageBoxW(mainWindow, L"Patching is complete. You may now close this window.", L"Message", MB_OK);	

}
