#pragma once
#include "pch.h"

class HookManager
{
    static bool isInitialized;
    /*static BOOL(__cdecl* HookFunction)(ULONG_PTR OriginalFunction, ULONG_PTR NewFunction);
    static VOID(__cdecl* UnhookFunction)(ULONG_PTR Function);*/

    HMODULE hLibrary = NULL;
    HANDLE hLogFile = NULL;
    LPTSTR lpProgramName = new TCHAR[MAX_PATH];
public:
    HookManager();
    ~HookManager();

    static ULONG_PTR(__cdecl* GetOriginalFunction)(ULONG_PTR Hook);

    void hookFunctions();
    void removeHooks();
};