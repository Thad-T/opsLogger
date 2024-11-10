#include "pch.h"
#include "HookManager.h"

bool HookManager::isInitialized = false;

static BOOL(__cdecl* HookFunction)(ULONG_PTR OriginalFunction, ULONG_PTR NewFunction);
static VOID(__cdecl* UnhookFunction)(ULONG_PTR Function);
ULONG_PTR(__cdecl* HookManager::GetOriginalFunction)(ULONG_PTR Hook);
// predefine hooked function below
DWORD NewFunction();

HookManager::HookManager()
{
	if (!isInitialized)
	{
		GetModuleFileName(NULL, lpProgramName, MAX_PATH);
		hLogFile = CreateFile(lpProgramName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (HookFunction == NULL || UnhookFunction == NULL || GetOriginalFunction == NULL)
		{
			HMODULE hHookEngineDll = LoadLibrary(L"NtHookEngine.dll");
			if (!hHookEngineDll)
			{
				WriteFile(hLogFile, "Error loading hooking engine", 29 * sizeof(char), NULL, NULL);
				return;
			}
			else
			{
				WriteFile(hLogFile, "Hooking engine loaded successfully", 35 * sizeof(char), NULL, NULL);
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
}

void HookManager::removeHooks()
{
	if (HookFunction != NULL && UnhookFunction != NULL && GetOriginalFunction != NULL && hLibrary != NULL)
	{
		UnhookFunction((ULONG_PTR)GetProcAddress(hLibrary, "GetAdaptersInfo"));
	}
}

void HookManager::hookFunctions()
{
	if (HookFunction == NULL || UnhookFunction == NULL || GetOriginalFunction == NULL)
		return;

	hLibrary = LoadLibrary(L""); // load library that contains function to be hooked
	if (hLibrary == NULL)
		return;

	HookFunction((ULONG_PTR)GetProcAddress(hLibrary, ""), (ULONG_PTR)NewFunction);
}