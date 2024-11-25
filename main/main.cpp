#include "framework.h" // framework.h contains a bunch of includes so any new includes should be added theres
#include "main.h"
// i think its best not to change these

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI InjectNewProc(__in LPCWSTR lpcwszDLL, __in LPCWSTR targetPath, LPCWSTR lpCurrentDir, HWND hwnd);
void PrintError(const char* lpFunction, HWND hwnd);

wchar_t selfdir[MAX_PATH] = { 0 };


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"opsLogWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"opsLogger",
        WS_CAPTION | WS_BORDER | WS_SYSMENU, // Fixed window styles
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, // Fixed size (300x200)
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    // Create a button
    CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Launch", // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, // Styles 
        100,         // x position 
        55,         // y position 
        100,        // Button width
        30,         // Button height
        hwnd,       // Parent window
        (HMENU)1,   // Control identifier
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // No additional application data

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        EndPaint(hwnd, &ps);
    }
    return 0;

    case WM_COMMAND: // Handle button click
        if (LOWORD(wParam) == 1) // Check if the button with ID 1 was clicked
        {
            GetModuleFileName(NULL, selfdir, MAX_PATH);
            PathCchRemoveFileSpec(selfdir, MAX_PATH);
            wchar_t hookingDll[MAX_PATH] = { 0 };
            PathCchCombineEx(hookingDll, MAX_PATH, selfdir, L"\HookingDll.dll", PATHCCH_NONE);

            wchar_t exePath[MAX_PATH];
            OPENFILENAME ofn;
            ZeroMemory(&exePath, sizeof(exePath));
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Executable Files\0*.exe\0Any File\0*.*\0";
            ofn.lpstrFile = exePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Select Program to Run";
            ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

            if (!GetOpenFileNameW(&ofn))
            {
                PrintError("GetOpenFileNameW", hwnd);
                break;
            }

            if (InjectNewProc(hookingDll, exePath, selfdir, hwnd))
                MessageBox(hwnd, L"Success", L"Button Click", MB_OK);
            else
                MessageBox(hwnd, L"Failed", L"Button Click", MB_OK);
        }
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}



BOOL WINAPI InjectNewProc(__in LPCWSTR lpcwszDLL, __in LPCWSTR targetPath, LPCWSTR lpCurrentDir, HWND hwnd)
{
    SIZE_T nLength;
    LPVOID lpLoadLibraryW = NULL;
    LPVOID lpRemoteString;
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInformation;

    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(STARTUPINFO);

    if (!CreateProcess(targetPath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, lpCurrentDir, &startupInfo, &processInformation))
    {
        PrintError("CreateProcess", hwnd);
        return false;
    }

    lpLoadLibraryW = GetProcAddress(GetModuleHandle(L"KERNEL32.DLL"), "LoadLibraryW");
    if (!lpLoadLibraryW)
    {
        PrintError("GetProcAddress", hwnd);
        CloseHandle(&processInformation.hThread);
        CloseHandle(&processInformation.hProcess);
        return false;
    }

    nLength = wcslen(lpcwszDLL) * sizeof(WCHAR);
    lpRemoteString = VirtualAllocEx(processInformation.hProcess, NULL, nLength + 1, MEM_COMMIT, PAGE_READWRITE);
    if (!lpRemoteString)
    {
        PrintError("VirtualAllocEx", hwnd);
        CloseHandle(&processInformation.hThread);
        CloseHandle(&processInformation.hProcess);
        return false;
    }

    if (!WriteProcessMemory(processInformation.hProcess, lpRemoteString, lpcwszDLL, nLength, NULL))
    {
        PrintError("WriteProcessMemory", hwnd);
        VirtualFreeEx(processInformation.hProcess, lpRemoteString, 0, MEM_RELEASE);
        CloseHandle(&processInformation.hThread);
        CloseHandle(&processInformation.hProcess);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(processInformation.hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)lpLoadLibraryW, lpRemoteString, NULL, NULL);
    if (!hThread)
    {
        PrintError("CreateRemoteThread", hwnd);
        VirtualFreeEx(processInformation.hProcess, lpRemoteString, 0, MEM_RELEASE);
        CloseHandle(&processInformation.hThread);
        CloseHandle(&processInformation.hProcess);
        return false;
    }
    else
    {
        WaitForSingleObject(hThread, 4000);
        ResumeThread(processInformation.hThread);
    }

    VirtualFreeEx(processInformation.hProcess, lpRemoteString, 0, MEM_RELEASE);
    CloseHandle(&processInformation.hThread);
    CloseHandle(&processInformation.hProcess);

    return true;
}

void PrintError(const char* lpFunction, HWND hwnd)
{
    LPVOID lpMsgBuf;
    LPVOID lpDispBuf;
    DWORD lastError = GetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, lastError, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPWSTR)&lpMsgBuf, 0, NULL);
    lpDispBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + 50) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDispBuf, LocalSize(lpDispBuf) / sizeof(TCHAR), TEXT("LoadLibrary failed with error code %d: %s"), lastError, (LPWSTR)lpMsgBuf);

    MessageBox(hwnd, (LPTSTR)lpDispBuf, L"Error", MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDispBuf);
}