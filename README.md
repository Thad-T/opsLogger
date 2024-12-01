# opsLogger

## Description

opsLogger is a tool that tracks the activity of an application while it is running.

## Requirements

In order for opsLogger to function properly, ensure that there are three items in the same folder:
1. The compiled executable of opsLogger
2. The compiled HookingDLL
3. A folder named "engine" containing the three hooking engine files (NtHookEngine.dll, NtHookEngine.exp, NtHookEngine.lib)

## Usage

To use opsLogger, simply run the opsLogger executable and run an application using the Launch button on the GUI.
A log file of the application's actions will be generated in the same directory as the opsLogger executable and its contents will be displayed on the GUI.
