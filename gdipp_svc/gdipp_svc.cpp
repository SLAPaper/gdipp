#include "stdafx.h"
#include "gdipp_svc.h"
#include <gdipp_common.h>

using namespace std;

SERVICE_STATUS			svc_status = {};
SERVICE_STATUS_HANDLE	svc_status_handle = NULL;
HANDLE					svc_stop_event = NULL;

svc_mon mon_instance;

#ifdef _M_X64
#define SVC_NAME TEXT("gdipp_svc_64")
#define SVC_EVENT_PREFIX L"Global\\gdipp_svc_event_64"
#else
#define SVC_NAME TEXT("gdipp_svc_32")
#define SVC_EVENT_PREFIX L"Global\\gdipp_svc_event_32"
#endif // _M_X64

VOID set_svc_status(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// fill in the SERVICE_STATUS structure
	svc_status.dwCurrentState = dwCurrentState;
	svc_status.dwWin32ExitCode = dwWin32ExitCode;
	svc_status.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		// no control is accepted in start pending state
		svc_status.dwControlsAccepted = 0;
	else
		svc_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
		svc_status.dwCheckPoint = 0;
	else
		svc_status.dwCheckPoint = dwCheckPoint++;

	// report the status of the service to the SCM
	SetServiceStatus(svc_status_handle, &svc_status);
}

VOID WINAPI svc_ctrl_handler(DWORD dwCtrl)
{
	// handle the requested control code
	switch (dwCtrl) 
	{
	case SERVICE_CONTROL_STOP:
		set_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		SetEvent(svc_stop_event);
		set_svc_status(svc_status.dwCurrentState, NO_ERROR, 0);
		return;
	}
}

bool create_svc_event(wstring &svc_name)
{
	// create named event to synchronize between service and gdimm

	// service event name = event name prefix + non-duplicable number (tick count)
	// use this dynamic event name to avoid existing same-named event, opened by lingering gdimm.dll
	wostringstream ss;
	ss << SVC_EVENT_PREFIX;
	ss << GetTickCount();

	svc_name = ss.str();

	svc_stop_event = CreateEventW(NULL, TRUE, FALSE, svc_name.c_str());
	if (svc_stop_event == NULL)
		return false;

	return true;
}

void initial_inject(const wchar_t *svc_name)
{
#ifdef _M_X64
	const wchar_t *gdipp_enum_name = L"gdipp_enum_64.exe";
#else
	const wchar_t *gdipp_enum_name = L"gdipp_enum_32.exe";
#endif

	BOOL b_ret;

	wchar_t gdipp_enum_path[MAX_PATH];
	b_ret = gdipp_get_dir_file_path(NULL, gdipp_enum_name, gdipp_enum_path);
	assert(b_ret);

	wstring cmd_line = gdipp_enum_path;
	cmd_line += L" --svc_name=";
	cmd_line += svc_name;

	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);
	PROCESS_INFORMATION pi;
	
	b_ret = CreateProcess(gdipp_enum_path, &cmd_line[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	if (b_ret)
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

VOID WINAPI svc_main(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// register the handler function for the service
	svc_status_handle = RegisterServiceCtrlHandler(SVC_NAME, svc_ctrl_handler);
	if (svc_status_handle == NULL)
		return;

	// these SERVICE_STATUS members remain as set here
	svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	svc_status.dwWin32ExitCode = NO_ERROR;

	// report initial status to the SCM
	set_svc_status(SERVICE_START_PENDING, NO_ERROR, INFINITE);

	wstring svc_name;
	if (!create_svc_event(svc_name))
	{
		set_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	gdipp_init_payload(GDIPP_SERVICE, svc_name.c_str());

	initial_inject(svc_name.c_str());

	// monitor future processes
	if (mon_instance.start_monitor())
	{
		// report running status when initialization is complete
		set_svc_status(SERVICE_RUNNING, NO_ERROR, 0);

		// wait for stop event
		WaitForSingleObject(svc_stop_event, INFINITE);

		set_svc_status(SERVICE_STOP_PENDING, NO_ERROR, 0);

		mon_instance.stop_monitor();
	}

	set_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
}

 // #define svc_debug

int APIENTRY wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPTSTR    lpCmdLine,
	int       nCmdShow)
{
#ifdef svc_debug
	wstring svc_name;
	if (!create_svc_event(svc_name))
	{
		set_svc_status(SERVICE_STOPPED, NO_ERROR, 0);
		return 0;
	}

	gdipp_init_payload(GDIPP_SERVICE, svc_name.c_str());

	initial_inject(svc_name.c_str());

	if (mon_instance.start_monitor())
	{
		Sleep(5000);
		mon_instance.stop_monitor();
	}
#else
	SERVICE_TABLE_ENTRY dispatch_table[] =
	{
		{ SVC_NAME, (LPSERVICE_MAIN_FUNCTION) svc_main },
		{ NULL, NULL },
	};

	StartServiceCtrlDispatcher(dispatch_table);
#endif // svc_debug

	return 0;
}