#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
            MessageBox(hwnd, L"i wan die", L"Button Click", MB_OK);
        }
        break;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
