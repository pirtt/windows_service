#include "windows_service_base.h"
#include <windows.h>
#include <assert.h>
#include <tchar.h>
#include <stdio.h>

windows_service_base *windows_service_base::global_service = NULL;

bool windows_service_base::run_service(windows_service_base &service)
{
	global_service = &service;

	SERVICE_TABLE_ENTRY service_table[] =
	{
		{ service.__service_name, (LPSERVICE_MAIN_FUNCTION)service_main},
		{ NULL, NULL }
	};

	return StartServiceCtrlDispatcher(service_table);
}

void WINAPI windows_service_base::service_main(DWORD argc, PTCHAR *argv)
{
	assert(global_service != NULL);

	global_service->__service_status_handle = RegisterServiceCtrlHandler(
		global_service->__service_name, service_ctrl);
	if (global_service->__service_status_handle == NULL)
	{
		throw GetLastError();
	}

	global_service->start_service(argc, argv);
}

windows_service_base::~windows_service_base(void)
{

}

void WINAPI windows_service_base::service_ctrl(DWORD request)
{
	switch (request)
	{
	case SERVICE_CONTROL_STOP: global_service->stop_service(); break;
	case SERVICE_CONTROL_PAUSE: global_service->pause_service(); break;
	case SERVICE_CONTROL_CONTINUE: global_service->continue_service(); break;
	case SERVICE_CONTROL_SHUTDOWN: global_service->shutdown_service(); break;
	case SERVICE_CONTROL_INTERROGATE: break;
	default: break;
	}
}

windows_service_base::windows_service_base(PTCHAR service_name,
	bool is_can_stop,
	bool is_can_shutdown, 
	bool is_can_pause_continue):
	__service_status_handle(NULL)
{ 
	__service_name = (service_name == NULL) ? TEXT("") : service_name;
	__service_status_handle = NULL; 
	__service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
	__service_status.dwCurrentState = SERVICE_START_PENDING; 
	DWORD dwControlsAccepted = 0;
	if (is_can_stop)
	{
		dwControlsAccepted |= SERVICE_ACCEPT_STOP;
	}	
	if (is_can_shutdown)
	{
		dwControlsAccepted |= SERVICE_ACCEPT_SHUTDOWN;
	}
	if (is_can_pause_continue)
	{
		dwControlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;
	}
	__service_status.dwControlsAccepted = dwControlsAccepted;
	__service_status.dwWin32ExitCode = NO_ERROR;
	__service_status.dwServiceSpecificExitCode = 0;
	__service_status.dwCheckPoint = 0;
	__service_status.dwWaitHint = 0;
}

void windows_service_base::start_service(DWORD argc, PTCHAR *argv)
{
	try
	{ 
		set_service_status(SERVICE_START_PENDING); 
		on_start(argc, argv);
		set_service_status(SERVICE_RUNNING);
	}
	catch (DWORD dwError)
	{ 
		write_log(TEXT("Service Start"), dwError);
		set_service_status(SERVICE_STOPPED, dwError);
	}
	catch (...)
	{
		write_log(TEXT("Service failed to start."), EVENTLOG_ERROR_TYPE);
		set_service_status(SERVICE_STOPPED);
	}
}

void windows_service_base::stop_service()
{
	DWORD laststatus = __service_status.dwCurrentState;
	try
	{ 
		set_service_status(SERVICE_STOP_PENDING); 
		on_stop(); 
		set_service_status(SERVICE_STOPPED);
	}
	catch (DWORD dwError)
	{
		write_log(TEXT("Service Stop"), dwError); 
		set_service_status(laststatus);
	}
	catch (...)
	{
		write_log(TEXT("Service failed to stop."), EVENTLOG_ERROR_TYPE);
		set_service_status(laststatus);
	}
}

void windows_service_base::pause_service()
{
	try
	{ 
		set_service_status(SERVICE_PAUSE_PENDING); 
		on_pause(); 
		set_service_status(SERVICE_PAUSED);
	}
	catch (DWORD dwError)
	{
		write_log(TEXT("Service Pause"), dwError); 
		set_service_status(SERVICE_RUNNING);
	}
	catch (...)
	{
		write_log(TEXT("Service failed to pause."), EVENTLOG_ERROR_TYPE); 
		set_service_status(SERVICE_RUNNING);
	}
}

void windows_service_base::continue_service()
{
	try
	{
		set_service_status(SERVICE_CONTINUE_PENDING); 
		on_continue(); 
		set_service_status(SERVICE_RUNNING);
	}
	catch (DWORD dwError)
	{
		write_log(TEXT("Service Continue"), dwError);
		set_service_status(SERVICE_PAUSED);
	}
	catch (...)
	{
		write_log(TEXT("Service failed to resume."), EVENTLOG_ERROR_TYPE); 
		set_service_status(SERVICE_PAUSED);
	}
}

void windows_service_base::shutdown_service()
{
	try
	{ 
		on_shutdown();
		set_service_status(SERVICE_STOPPED);
	}
	catch (DWORD dwError)
	{
		write_log(TEXT("Service Shutdown"), dwError);
	}
	catch (...)
	{
		write_log(TEXT("Service failed to shut down."), EVENTLOG_ERROR_TYPE);
	}
}

void windows_service_base::set_service_status(DWORD dwCurrentState,
	 DWORD exit_code, DWORD wait_hint)
{
	static DWORD dwCheckPoint = 1;
	__service_status.dwCurrentState = dwCurrentState;
	__service_status.dwWin32ExitCode = exit_code;
	__service_status.dwWaitHint = wait_hint;

	__service_status.dwCheckPoint =
		((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED)) ?
		0 : dwCheckPoint++;
	::SetServiceStatus(__service_status_handle, &__service_status);
}

void windows_service_base::write_log(PTCHAR log_context, WORD type)
{
	on_write_log(log_context, type);
}

void windows_service_base::on_write_log(PTCHAR log_context, WORD type)
{
	HANDLE hEventSource = NULL;
	LPCTSTR log[2] = { NULL, NULL };

	hEventSource = RegisterEventSource(NULL, __service_name);
	if (hEventSource)
	{
		log[0] = __service_name;
		log[1] = log_context;

		ReportEvent(hEventSource, type,
			0,                     // Event category 
			0,                     // Event identifier 
			NULL,                  // No security identifier 
			2,                     // Size of lpszStrings array 
			0,                     // No binary data 
			log,				   // Array of strings 
			NULL                   // No binary data 
		);

		DeregisterEventSource(hEventSource);
	}
}


void install_service(PTCHAR service_name,
	PTCHAR display_name,
	DWORD  start_type,
	PTCHAR dependencies,
	PTCHAR accout,
	PTCHAR passwd)
{
	TCHAR szPath[MAX_PATH];
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)) == 0)
	{
		printf("GetModuleFileName failed [%d]\n", GetLastError());
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}
		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}
	}

	// Open the local default service control manager database 
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT |
		SC_MANAGER_CREATE_SERVICE);
	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed [%d]\n", GetLastError());
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}
		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}
	}
 
	schService = CreateService(
		schSCManager,					// SCManager database 
		service_name,					// Name of service 
		display_name,					// Name to display 
		SERVICE_QUERY_STATUS,           // Desired access 
		SERVICE_WIN32_OWN_PROCESS,      // Service type 
		start_type,						// Service start type 
		SERVICE_ERROR_NORMAL,           // Error control type 
		szPath,                         // Service's binary 
		NULL,                           // No load ordering group 
		NULL,                           // No tag identifier 
		dependencies,					// Dependencies 
		accout,							// Service running account 
		passwd							// Password of the account 
	);
	if (schService == NULL)
	{
		printf("CreateService failed [%d]\n", GetLastError());
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}
		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}
	}
	printf("%s is installed.\n", service_name);
}


void uninstall_service(PTCHAR service_name)
{
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;
	SERVICE_STATUS ssSvcStatus = {};

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager == NULL)
	{
		printf("OpenSCManager failed [%d]\n", GetLastError());
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}
		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}
	}

	// Open the service with delete, stop, and query status permissions 
	schService = OpenService(schSCManager, service_name, SERVICE_STOP |
		SERVICE_QUERY_STATUS | DELETE);
	if (schService == NULL)
	{
		printf("OpenSCManager failed [%d]\n", GetLastError());
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}
		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}
	}

	// Try to stop the service 
	if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
	{
		printf("Stopping %s.", service_name);
		Sleep(1000);
		while (QueryServiceStatus(schService, &ssSvcStatus))
		{
			if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
			{
				printf(".");
				Sleep(1000);
			}
			else break;
		}

		if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED)
		{
			printf("\n%s is stopped.\n", service_name);
		}
		else
		{
			printf("\n%s failed to stop.\n", service_name);
		}
	}

	// Now remove the service by calling DeleteService. 
	if (!DeleteService(schService))
	{
		printf("DeleteService failed [%d]\n", GetLastError());
		if (schSCManager)
		{
			CloseServiceHandle(schSCManager);
			schSCManager = NULL;
		}
		if (schService)
		{
			CloseServiceHandle(schService);
			schService = NULL;
		}
	}

	printf("%s is removed.\n", service_name);	
}