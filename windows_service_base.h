#ifndef WINDOWS_SERVICE_BASE
#define WINDOWS_SERVICE_BASE

#include <windows.h> 
#include <tchar.h>

/****************************************************
*	Windows Service Base Class
*	Author:Yoohoo Niu
*	Copyright (c) Yoohoo,wand_niu@live.com
*****************************************************/

class windows_service_base
{
public:
	static bool run_service(windows_service_base &service);
	windows_service_base(PTCHAR service_name, bool is_can_stop = true,
		bool is_can_shutdown = true, bool is_can_pause_continue = false);
	virtual ~windows_service_base(void);
	void stop_service();
protected:
	virtual void on_start(DWORD argc, PTCHAR *argv) {};
	virtual void on_stop() {};
	virtual void on_pause() {};
	virtual void on_continue() {};
	virtual void on_shutdown() {};
	virtual void on_write_log(PTCHAR log_context, WORD type);
private:
	void set_service_status(DWORD current_status, DWORD exit_code = NO_ERROR, DWORD wait_hint = 0);
	static void WINAPI service_main(DWORD argc, PTCHAR *argv);
	static void WINAPI service_ctrl(DWORD request);
	void start_service(DWORD argc, PTCHAR *argv);
	void write_log(PTCHAR log_context, WORD type);
	void pause_service();
	void continue_service();
	void shutdown_service();
	static windows_service_base *global_service;
	PTCHAR 					__service_name;
	SERVICE_STATUS 			__service_status;
	SERVICE_STATUS_HANDLE 	__service_status_handle;
};

void install_service(PTCHAR service_name,
	PTCHAR display_name,
	DWORD  start_type,
	PTCHAR dependencies,
	PTCHAR accout,
	PTCHAR passwd);

void uninstall_service(PTCHAR service_name);

#endif