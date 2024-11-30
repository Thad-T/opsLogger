#include "pch.h"
#include "HookManager.h"

bool HookManager::isInitialized = false;

DWORD selfPid = GetCurrentProcessId();
HANDLE hLogFile = NULL;
wchar_t lpHookingDll[MAX_PATH] = { 0 };

static BOOL(__cdecl* HookFunction)(ULONG_PTR OriginalFunction, ULONG_PTR NewFunction);
static VOID(__cdecl* UnhookFunction)(ULONG_PTR Function);
ULONG_PTR(__cdecl* HookManager::GetOriginalFunction)(ULONG_PTR Hook);

BOOL InjectProc(LPPROCESS_INFORMATION lpProcInfo);
void PrintError(const char* lpFunction);
void PrintAction(LPCSTR lpAction, LPCSTR lpDesc);

// predefine hooked function below
HANDLE HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL HookedWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
DWORD HookedProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

HookManager::HookManager()
{
	if (!isInitialized)
	{
		LPTSTR lpProgramPath = new TCHAR[MAX_PATH];
		LPTSTR lpProgramName = new TCHAR[MAX_PATH];
		LPTSTR lpWorkingDir = new TCHAR[MAX_PATH];
		LPTSTR lpLogFile = new TCHAR[MAX_PATH];
		LPTSTR lpTempDir = new TCHAR[MAX_PATH];
		BOOL dirChange = false;
		GetModuleFileName(NULL, lpProgramPath, MAX_PATH);
		GetCurrentDirectory(MAX_PATH, lpWorkingDir);
		PathCchCombineEx(lpHookingDll, MAX_PATH, lpWorkingDir, L"\HookingDll.dll", PATHCCH_NONE);
		lpTempDir = lpWorkingDir;
		HANDLE hDirTest = CreateFile(lpHookingDll, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (hDirTest == INVALID_HANDLE_VALUE)
		{
			dirChange = true;
			HMODULE hHookingDll = GetModuleHandle(L"HookingDll.dll");
			GetModuleFileName(hHookingDll, lpWorkingDir, MAX_PATH);
			PathCchRemoveFileSpec(lpWorkingDir, MAX_PATH);
		}
		else
			CloseHandle(hDirTest);
		lpProgramName = PathFindFileName(lpProgramPath);
		StringCchPrintf(lpLogFile, MAX_PATH, TEXT("%d-%s"), selfPid, lpProgramName);
		PathCchCombineEx(lpLogFile, MAX_PATH, lpWorkingDir, lpLogFile, PATHCCH_NONE);
		PathCchRenameExtension(lpLogFile, MAX_PATH, L"log");
		hLogFile = CreateFile(lpLogFile, GENERIC_WRITE | FILE_SHARE_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (!hLogFile)
		{
			PrintError("CreateFile");
			return;
		}
			
		WriteFile(hLogFile, "System Time,Local Time,Action,Description", 42 * sizeof(char), NULL, NULL);

		if (HookFunction == NULL || UnhookFunction == NULL || GetOriginalFunction == NULL)
		{
			HMODULE hHookEngineDll;
			if (dirChange)
			{
				SetCurrentDirectory(lpWorkingDir);
				hHookEngineDll = LoadLibrary(L".\\engine\\NtHookEngine.dll");
				SetCurrentDirectory(lpTempDir);
			}
			else
				hHookEngineDll = LoadLibrary(L".\\engine\\NtHookEngine.dll");
			
			if (!hHookEngineDll)
			{
				PrintAction("Initialization", "Hooking engine failed to load");
				CloseHandle(hLogFile);
				return;
			}
			else
			{
				PrintAction("Initialization", "Hooking engine loaded successfully");
			}

			HookFunction = (BOOL(__cdecl*)(ULONG_PTR, ULONG_PTR)) GetProcAddress(hHookEngineDll, "HookFunction");
			UnhookFunction = (VOID(__cdecl*)(ULONG_PTR)) GetProcAddress(hHookEngineDll, "UnhookFunction");
			GetOriginalFunction = (ULONG_PTR(__cdecl*)(ULONG_PTR)) GetProcAddress(hHookEngineDll, "GetOriginalFunction");
		}

		isInitialized = true;

		hookFunctions();
	}
}

HookManager::~HookManager()
{
	removeHooks();
	CloseHandle(hLogFile);
}

void HookManager::removeHooks()
{
	if (HookFunction != NULL && UnhookFunction != NULL && GetOriginalFunction != NULL && hKern32 != NULL && hKernBase != NULL)
	{
		UnhookFunction((ULONG_PTR)GetProcAddress(hKern32, "CreateProcessW"));
		UnhookFunction((ULONG_PTR)GetProcAddress(hKernBase, "CreateFileW"));
		UnhookFunction((ULONG_PTR)GetProcAddress(hKernBase, "WriteFile"));
	}
}

void HookManager::hookFunctions()
{
	if (HookFunction == NULL || UnhookFunction == NULL || GetOriginalFunction == NULL)
		return;

	hKern32 = LoadLibrary(L"Kernel32.dll");
	hKernBase = LoadLibrary(L"KernelBase.dll");
	if (hKern32 == NULL)
	{
		PrintAction("Initialization", "Failed to load Kernel32.dll");
	}
	else
	{
		if (!HookFunction((ULONG_PTR)GetProcAddress(hKern32, "CreateProcessW"), (ULONG_PTR)HookedProcessW))
		{
			PrintAction("Initialization", "Failed to hook CreateProcessW");
		}
	}
	if (hKernBase == NULL)
	{
		PrintAction("Initialization", "Failed to load KernelBase.dll");
	}
	else
	{
		if (!HookFunction((ULONG_PTR)GetProcAddress(hKernBase, "CreateFileW"), (ULONG_PTR)HookedCreateFileW))
		{
			PrintAction("Initialization", "Failed to hook CreateFileW");
		}
		if (!HookFunction((ULONG_PTR)GetProcAddress(hKernBase, "WriteFile"), (ULONG_PTR)HookedWriteFile))
		{
			PrintAction("Initialization", "Failed to hook WriteFile");
		}
	}
}

HANDLE HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE(*OGCreateFileW)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
	OGCreateFileW = (HANDLE(*)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile))HookManager::GetOriginalFunction((ULONG_PTR)HookedCreateFileW);
	
	LPCSTR lpSuccess = "True";
	char lpTemplate[MAX_PATH] = { 0 };
	char lpFileNameA[MAX_PATH] = { 0 };
	WideCharToMultiByte(CP_UTF8, 0, lpFileName, lstrlen(lpFileName), lpFileNameA, MAX_PATH * sizeof(char), NULL, NULL);
	HANDLE ret = OGCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	if (ret == INVALID_HANDLE_VALUE)
		lpSuccess = "False";
	if (hTemplateFile != NULL)
		GetFinalPathNameByHandleA(hTemplateFile, lpTemplate, MAX_PATH, FILE_NAME_NORMALIZED);
	else
		StringCchPrintfA(lpTemplate, MAX_PATH, "No Template File Specified");

	DWORD descSz = lstrlenA(lpSuccess) + lstrlenA(lpFileNameA) + lstrlenA(lpTemplate) + 119;
	LPSTR lpDesc = (LPSTR)LocalAlloc(LMEM_ZEROINIT, descSz * sizeof(char));
	StringCchPrintfExA(lpDesc, descSz, NULL, NULL, 0x00000220,
		"Success: %s;File Name: %s;Desired Access: %X;Share Mode: %X;Disposition: %X;Flags and Attributes: %X;Template File: %s",
		lpSuccess, lpFileNameA, dwDesiredAccess, dwShareMode, dwCreationDisposition, dwFlagsAndAttributes, lpTemplate);
	
	PrintAction("Create File", lpDesc);
	LocalFree(lpDesc);
	return ret;
}

BOOL HookedWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	BOOL(*OGWriteFile)(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
	OGWriteFile = (BOOL(*)(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped))HookManager::GetOriginalFunction((ULONG_PTR)HookedWriteFile);
	
	if (hFile == hLogFile)
		return OGWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);

	LPCSTR lpSuccess = "True";
	char lpFilePath[MAX_PATH] = { 0 };
	if (GetFinalPathNameByHandleA(hFile, lpFilePath, MAX_PATH, FILE_NAME_NORMALIZED) == 0)
	{
		switch (GetLastError())
		{
		case ERROR_NOT_ENOUGH_MEMORY:
			StringCchPrintfA(lpFilePath, MAX_PATH, "Insufficient memory to complete the operation");
			break;

		case ERROR_PATH_NOT_FOUND:
			StringCchPrintfA(lpFilePath, MAX_PATH, "Path not found");
			break;

		case ERROR_INVALID_PARAMETER:
			StringCchPrintfA(lpFilePath, MAX_PATH, "Invalid flags were specified for dwFlags");
			break;

		default:
			StringCchPrintfA(lpFilePath, MAX_PATH, "Unknown error occurred when retrieving file name");
			break;
		}
	}
	BOOL ret = OGWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
	if (!ret)
		lpSuccess = "False";

	DWORD descSz = lstrlenA(lpFilePath) + 70;
	LPSTR lpDesc = (LPSTR)LocalAlloc(LMEM_ZEROINIT, descSz * sizeof(char));
	StringCchPrintfExA(lpDesc, descSz, NULL, NULL, 0x00000220,
		"Success: %s;Target File Path: %s;Bytes to Write: %d;Bytes Written: %d",
		lpSuccess, lpFilePath, nNumberOfBytesToWrite, lpNumberOfBytesWritten);

	PrintAction("Write File", lpDesc);
	LocalFree(lpDesc);
	return ret;
}

DWORD HookedProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	DWORD(*OGCreateProcess)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);
	OGCreateProcess = (DWORD(*)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation))HookManager::GetOriginalFunction((ULONG_PTR)HookedProcessW);
	
	LPCSTR lpSuccess = "True";
	char lpAppNameA[MAX_PATH] = { 0 };
	char lpCurrentDirA[MAX_PATH] = { 0 };
	int cmdSz = lstrlen(lpCommandLine);
	LPSTR lpCmdLineA = (LPSTR)LocalAlloc(LMEM_ZEROINIT, cmdSz * sizeof(char));
	DWORD dwChildPid = -1;
	DWORD ret = OGCreateProcess(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags | CREATE_SUSPENDED, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
	WideCharToMultiByte(CP_UTF8, 0, lpApplicationName, lstrlen(lpApplicationName), lpAppNameA, MAX_PATH * sizeof(char), NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, lpCommandLine, cmdSz, lpCmdLineA, cmdSz * sizeof(char), NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, lpCurrentDirectory, lstrlen(lpCurrentDirectory), lpCurrentDirA, MAX_PATH * sizeof(char), NULL, NULL);
	if (ret == 0)
		lpSuccess = "False";
	else
	{
		dwChildPid = lpProcessInformation->dwProcessId;
		InjectProc(lpProcessInformation);
	}

	DWORD descSz = lstrlenA(lpSuccess) + lstrlenA(lpAppNameA) + lstrlenA(lpCurrentDirA) + lstrlenA(lpCmdLineA) + 86;
	LPSTR lpDesc = (LPSTR)LocalAlloc(LMEM_ZEROINIT, descSz * sizeof(char));
	StringCchPrintfExA(lpDesc, descSz, NULL, NULL, 0x00000220,
		"Success: %s;Application Name: %s;Command Line: %s;Current Directory: %s;Child PID: %d",
		lpSuccess, lpAppNameA, lpCmdLineA, lpCurrentDirA, dwChildPid);

	PrintAction("Create Process", lpDesc);
	LocalFree(lpCmdLineA);
	LocalFree(lpDesc);
	return ret;
}

BOOL InjectProc(LPPROCESS_INFORMATION lpProcInfo)
{
	SIZE_T nLength;
	LPVOID lpLoadLibraryW = NULL;
	LPVOID lpRemoteString;

	lpLoadLibraryW = GetProcAddress(GetModuleHandle(L"KERNEL32.DLL"), "LoadLibraryW");
	if (!lpLoadLibraryW)
	{
		ResumeThread(lpProcInfo->hThread);
		return false;
	}

	nLength = wcslen(lpHookingDll) * sizeof(WCHAR);
	lpRemoteString = VirtualAllocEx(lpProcInfo->hProcess, NULL, nLength + 1, MEM_COMMIT, PAGE_READWRITE);
	if (!lpRemoteString)
	{
		ResumeThread(lpProcInfo->hThread);
		return false;
	}

	if (!WriteProcessMemory(lpProcInfo->hProcess, lpRemoteString, lpHookingDll, nLength, NULL))
	{
		VirtualFreeEx(lpProcInfo->hProcess, lpRemoteString, 0, MEM_RELEASE);
		ResumeThread(lpProcInfo->hThread);
		return false;
	}

	HANDLE hThread = CreateRemoteThread(lpProcInfo->hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)lpLoadLibraryW, lpRemoteString, NULL, NULL);
	if (!hThread)
	{
		VirtualFreeEx(lpProcInfo->hProcess, lpRemoteString, 0, MEM_RELEASE);
		ResumeThread(lpProcInfo->hThread);
		return false;
	}
	else
	{
		WaitForSingleObject(hThread, 4000);
		ResumeThread(lpProcInfo->hThread);
	}

	VirtualFreeEx(lpProcInfo->hProcess, lpRemoteString, 0, MEM_RELEASE);
	return true;
}

void PrintError(const char* lpFunction)
{
	LPVOID lpMsgBuf;
	LPVOID lpDispBuf;
	DWORD lastError = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, lastError, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&lpMsgBuf, 0, NULL);
	lpDispBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + 50) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDispBuf, LocalSize(lpDispBuf) / sizeof(TCHAR), TEXT("LoadLibrary failed with error code %d: %s"), lastError, (LPWSTR)lpMsgBuf);

	MessageBox(NULL, (LPTSTR)lpDispBuf, L"Error", MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDispBuf);
}

void PrintAction(LPCSTR lpAction, LPCSTR lpDesc)
{
	SYSTEMTIME st, lt;
	GetSystemTime(&st);
	GetLocalTime(&lt);
	LPSTR lpDispBuf;
	char *lpDispBufEx;
	DWORD dispSz = lstrlenA(lpAction) + lstrlenA(lpDesc) + 65;
	lpDispBuf = (LPSTR)LocalAlloc(LMEM_ZEROINIT, dispSz * sizeof(char));
	StringCchPrintfExA(lpDispBuf, dispSz, &lpDispBufEx, NULL, 0x00000220,
		("\r\n%04d-%02d-%02d %02d:%02d:%02d:%03d,%04d-%02d-%02d %02d:%02d:%02d:%03d,%s,%s"),
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds,
		lpAction, lpDesc);
	lpDispBufEx[0] = ' ';
	WriteFile(hLogFile, lpDispBuf, dispSz * sizeof(char), NULL, NULL);
	SetEndOfFile(hLogFile);
	LocalFree(lpDispBuf);
}