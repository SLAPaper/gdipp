// notepad.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <easyhook.h>
#include <shlwapi.h>
#include <tlhelp32.h>

DWORD get_proc_id(LPCTSTR proc_name)
{
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);
	HANDLE h_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	assert(h_snapshot != INVALID_HANDLE_VALUE);
	
	if (Process32First(h_snapshot, &pe32))
	{
		do
		{
			if (lstrcmpi(pe32.szExeFile, proc_name) == 0)
			{
				CloseHandle(h_snapshot);
				return pe32.th32ProcessID;
			}

		} while (Process32Next(h_snapshot, &pe32));
	}

	CloseHandle(h_snapshot);
	return -1;
}

DWORD load_process(LPCTSTR proc_name)
{
	STARTUPINFO si = {0};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi = {0};

	BOOL b_ret = CreateProcess(proc_name, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	assert(b_ret == TRUE);

	return pi.dwProcessId;
}

void inject_dll(DWORD proc_id, LPTSTR dll_path)
{
	NTSTATUS eh_error = RhInjectLibrary(proc_id, 0, EASYHOOK_INJECT_DEFAULT, dll_path, NULL, NULL, 0);
	assert(eh_error == 0);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	LPCTSTR target_proc = TEXT("dwm.exe");
	LPCTSTR target_path = TEXT("C:\\Windows\\notepad.exe");

	DWORD dw_ret;
	BOOL b_ret;

	TCHAR hook_dll[MAX_PATH];
	dw_ret = GetModuleFileName(NULL, hook_dll, MAX_PATH);
	assert(dw_ret != 0);

	b_ret = PathRemoveFileSpec(hook_dll);
	assert(b_ret == TRUE);

	b_ret = PathAppend(hook_dll, TEXT("dll.dll"));
	assert(b_ret == TRUE);

	DWORD proc_id = get_proc_id(target_proc);
	if (proc_id == -1)
		proc_id = load_process(target_path);

	inject_dll(proc_id, hook_dll);

	return 0;
}
