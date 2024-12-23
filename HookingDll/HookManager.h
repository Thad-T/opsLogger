#pragma once
#include "pch.h"



class HookManager
{
    static bool isInitialized;
    /*static BOOL(__cdecl* HookFunction)(ULONG_PTR OriginalFunction, ULONG_PTR NewFunction);
    static VOID(__cdecl* UnhookFunction)(ULONG_PTR Function);*/

    HMODULE hKern32 = NULL;
    HMODULE hKernBase = NULL;

public:
    HookManager();
    ~HookManager();

    static ULONG_PTR(__cdecl* GetOriginalFunction)(ULONG_PTR Hook);

    void hookFunctions();
    void removeHooks();

};