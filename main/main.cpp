#include "framework.h" // framework.h contains a bunch of includes so any new includes should be added theres
#include "main.h"
// i think its best not to change these

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
std::wstring FormatCSV(const std::string& csvContent, const std::string& filename);
void MonitorFileChanges(HWND hwnd, const std::string& filePath, HWND hEditControl);
void UpdateFileContent(HWND hwnd, const std::string& filePath, HWND hEditControl);
void PopulateComboBox(HWND hComboBox, const std::string& directoryPath);
void RefreshComboBox(HWND hComboBox, const std::string& directoryPath);
void MonitorDirectoryChanges(HWND hwnd, HWND hComboBox, const std::string& directoryPath);
std::vector<std::string> GetLogFiles(const std::string& directoryPath);
std::string GetExecutablePath();
// Global variable to track the file size for change detection
std::atomic<long> totalFileSize(0);
std::string directoryPath = GetExecutablePath();

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
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, // Fixed size (600x400)
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    // Create an EDIT control for displaying the formatted content
    HWND hEditControl = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN | ES_READONLY,
        20,
        70,
        540,
        280,
        hwnd,
        (HMENU)2,
        hInstance,
        NULL
    );

    // Create a combobox (dropdown) for file selection
    HWND hComboBox = CreateWindowEx(
        0,
        L"COMBOBOX",
        NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        20,
        20,
        200,
        100,
        hwnd,
        (HMENU)3,
        hInstance,
        NULL
    );
    PopulateComboBox(hComboBox, ".");

    CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Refresh", // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, // Styles 
        360,         // x position 
        20,         // y position 
        100,        // Button width
        30,         // Button height
        hwnd,       // Parent window
        (HMENU)4,   // Control identifier
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // No additional application data

    // Create a button
    CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Launch", // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, // Styles 
        250,         // x position 
        20,         // y position 
        100,        // Button width
        30,         // Button height
        hwnd,       // Parent window
        (HMENU)1,   // Control identifier
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // No additional application data

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Monitor the log file for changes in a separate thread
    // std::string directoryPath = "."; // Current directory
    std::thread directoryMonitorThread(MonitorDirectoryChanges, hwnd, hComboBox, directoryPath);
    directoryMonitorThread.detach();
    std::thread fileMonitorThread(MonitorFileChanges, hwnd, directoryPath, hEditControl);
    fileMonitorThread.detach();

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
    HWND hComboBox = GetDlgItem(hwnd, 3);
    switch (uMsg)
    {
    case WM_USER + 1: // Custom message for refreshing the combobox
        if (hComboBox != NULL)
        {
            RefreshComboBox(hComboBox, directoryPath);
        }
        break;
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

            // std::string directoryPath = "."; // Current directory
            HWND hEditControl = GetDlgItem(hwnd, 2);
            UpdateFileContent(hwnd, directoryPath, hEditControl);
        }
        else if (LOWORD(wParam) == 4) // Refresh button
        {
            // Refresh the file list in the combobox
            if (hComboBox != NULL)
            {
                RefreshComboBox(hComboBox, directoryPath);

                // Optional: Also refresh the displayed content
                HWND hEditControl = GetDlgItem(hwnd, 2);
                UpdateFileContent(hwnd, directoryPath, hEditControl);
            }
        }
        else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == 3) // 3 is the combobox ID
        {
            // Get the selected item index
            int index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR)
            {
                wchar_t selectedFile[256];
                SendMessage((HWND)lParam, CB_GETLBTEXT, index, (LPARAM)selectedFile);

                std::wstring wideFileName(selectedFile);
                std::string selectedFilename(wideFileName.begin(), wideFileName.end());

                std::string filePath = directoryPath + selectedFilename; // Adjust for your directory structure
                std::ifstream file(filePath);
                HWND hEditControl = GetDlgItem(hwnd, 2);

                if (selectedFilename == "All Files")
                {
                    std::wstringstream combinedContent;
                    std::vector<std::string> logFiles = GetLogFiles(directoryPath);

                    for (const auto& filePath : logFiles)
                    {
                        std::ifstream file(filePath);
                        if (file.is_open())
                        {
                            std::stringstream buffer;
                            buffer << file.rdbuf();
                            std::string fileContent = buffer.str();

                            std::string filename = filePath.substr(filePath.find_last_of("\\/") + 1);

                            combinedContent << FormatCSV(fileContent, filename);
                        }
                    }

                    SetWindowText(hEditControl, combinedContent.str().c_str());
                }
                else
                {
                    // View selected file
                    std::string filePath = directoryPath + "//" + selectedFilename;
                    std::ifstream file(filePath);
                    if (file.is_open())
                    {
                        std::stringstream buffer;
                        buffer << file.rdbuf();
                        std::string fileContent = buffer.str();
                        std::wstring formattedContent = FormatCSV(fileContent, selectedFilename);

                        SetWindowText(hEditControl, formattedContent.c_str());
                    }
                    else
                    {
                        MessageBox(hwnd, L"Failed to open the selected file.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
            }
        }
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

// Function to get all .log files in a directory
std::vector<std::string> GetLogFiles(const std::string& directoryPath)
{
    std::vector<std::string> logFiles;
    WIN32_FIND_DATAW findData; // Use wide-character version
    std::wstring wideDirectoryPath = std::wstring(directoryPath.begin(), directoryPath.end());
    std::wstring searchPattern = wideDirectoryPath + L"\\*.log";

    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                // Convert wide file name to narrow string
                std::wstring wideFileName = findData.cFileName;
                std::string fileName(wideFileName.begin(), wideFileName.end());
                logFiles.push_back(directoryPath + "//" + fileName);
            }
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }

    return logFiles;
}

// Function to format the CSV content into a readable table
std::wstring FormatCSV(const std::string& csvContent, const std::string& filename)
{
    std::wstringstream formattedContent;
    std::vector<std::vector<std::string>> rows;
    std::istringstream csvStream(csvContent);
    std::string line;

    bool isFirstRow = true; // Flag to track the first row

    // Add filename as a header
    formattedContent << L"Filename: " << std::wstring(filename.begin(), filename.end()) << L"\r\n";
    formattedContent << L"System Time                     Local Time                        Action             Description\r\n";

    // Read CSV content, split by newlines first, and then split by commas
    while (std::getline(csvStream, line)) // Split into lines by newlines
    {
        if (isFirstRow) // Skip the first row
        {
            isFirstRow = false;
            continue;
        }

        std::istringstream lineStream(line);
        std::vector<std::string> row;
        std::string cell;

        // Split the line by commas
        while (std::getline(lineStream, cell, ','))
        {
            row.push_back(cell);
        }
        rows.push_back(row); // Add the row to the table
    }

    // Determine the maximum width for each column
    std::vector<size_t> columnWidths;
    for (const auto& row : rows)
    {
        for (size_t i = 0; i < row.size(); ++i)
        {
            if (i >= columnWidths.size())
                columnWidths.push_back(0);
            columnWidths[i] = std::max(columnWidths[i], static_cast<size_t>(row[i].length()));
        }
    }

    // Format each row with aligned columns
    for (const auto& row : rows)
    {
        for (size_t i = 0; i < row.size(); ++i)
        {
            // Trim leading and trailing spaces from each cell
            std::wstring cellStr(row[i].begin(), row[i].end());
            cellStr.erase(0, cellStr.find_first_not_of(L' ')); // Remove leading spaces
            cellStr.erase(cellStr.find_last_not_of(L' ') + 1); // Remove trailing spaces

            formattedContent << std::setw(columnWidths[i]) << std::left << cellStr;
            if (i < row.size() - 1)
                formattedContent << L"  "; // Add spacing between columns
        }
        formattedContent << L"\r\n"; // Add newline for each row
    }

    formattedContent << L"\r\n"; // Add a newline between files
    return formattedContent.str();
}

// Function to monitor the log files for changes and update the display
void MonitorFileChanges(HWND hwnd, const std::string& directoryPath, HWND hEditControl)
{
    while (true)
    {
        std::vector<std::string> logFiles = GetLogFiles(directoryPath);
        long currentTotalSize = 0;

        for (const auto& filePath : logFiles)
        {
            std::ifstream file(filePath, std::ios::ate);
            if (file.is_open())
            {
                currentTotalSize += file.tellg();
            }
        }

        // If the total file size has changed, we have new data
        if (currentTotalSize > totalFileSize)
        {
            totalFileSize = currentTotalSize;
            UpdateFileContent(hwnd, directoryPath, hEditControl);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Check every 1 second
    }
}

// Function to read the updated content from all .log files and update the display
void UpdateFileContent(HWND hwnd, const std::string& directoryPath, HWND hEditControl)
{
    std::vector<std::string> logFiles = GetLogFiles(directoryPath);
    std::wstringstream combinedContent;

    for (const auto& filePath : logFiles)
    {
        std::ifstream file(filePath);
        if (file.is_open())
        {
            std::stringstream buffer;
            buffer << file.rdbuf(); // Read file into a string
            std::string fileContent = buffer.str();

            // Extract filename from filePath
            std::string filename = filePath.substr(filePath.find_last_of("\\/") + 1);

            // Append formatted content to combined output
            combinedContent << FormatCSV(fileContent, filename);
        }
    }

    // Update the content in the EDIT control
    SetWindowText(hEditControl, combinedContent.str().c_str());
}

void PopulateComboBox(HWND hComboBox, const std::string& directoryPath)
{
    SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"All Files");
    std::vector<std::string> logFiles = GetLogFiles(directoryPath);
    for (const auto& filePath : logFiles)
    {
        // Extract filename from filePath
        std::string filename = filePath.substr(filePath.find_last_of("\\/") + 1);
        std::wstring wideFilename(filename.begin(), filename.end());

        // Add each filename to the combobox
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)wideFilename.c_str());
    }
    SendMessage(hComboBox, CB_SETCURSEL, 0, 0);
}

void RefreshComboBox(HWND hComboBox, const std::string& directoryPath)
{
    // Clear existing entries
    SendMessage(hComboBox, CB_RESETCONTENT, 0, 0);

    // Repopulate the combobox with updated file list
    PopulateComboBox(hComboBox, directoryPath);
}

void MonitorDirectoryChanges(HWND hwnd, HWND hComboBox, const std::string& directoryPath)
{
    std::vector<std::string> previousFiles = GetLogFiles(directoryPath);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Check every 2 seconds

        std::vector<std::string> currentFiles = GetLogFiles(directoryPath);

        // Compare the current file list with the previous one
        if (currentFiles != previousFiles)
        {
            previousFiles = currentFiles;

            // Refresh the combobox on the main thread
            PostMessage(hwnd, WM_USER + 1, (WPARAM)hComboBox, 0);
        }
    }
}

std::string GetExecutablePath() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH); // Use the narrow version
    std::string fullPath(buffer);
    return fullPath.substr(0, fullPath.find_last_of("\\/"));
}