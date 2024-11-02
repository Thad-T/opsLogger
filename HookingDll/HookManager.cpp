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
		if (HookFunction == NULL || UnhookFunction == NULL || GetOriginalFunction == NULL)
		{
			HMODULE hHookEngineDll = LoadLibrary(L"NtHookEngine.dll");

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

void HookManager::hookFunctions()
{
	if (HookFunction == NULL || UnhookFunction == NULL || GetOriginalFunction == NULL)
		return;

	hLibrary = LoadLibrary(L""); // load library that contains function to be hooked
	if (hLibrary == NULL)
		return;

	HookFunction((ULONG_PTR)GetProcAddress(hLibrary, ""), (ULONG_PTR)NewFunction);
}