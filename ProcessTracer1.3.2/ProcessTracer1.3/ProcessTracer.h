#pragma once
/*
ProcessTracer.h
ProcessTracer class 선언부분
디버깅하는데 있어서 핵심 함수 및 변수를 포함
*/
static char const szRCSID[] = "$Id: ProcessTracer.cpp 84 2011-11-13 00:29:15Z Roger $";
#ifdef _M_X64
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif // _M_X64

#include <windows.h>
#include <map>
#include "SimpleSymbolEngine.h"

class ProcessTracer
{
private:
	HANDLE hProcess;
	std::map<DWORD, HANDLE> threadHandles;
	SimpleSymbolEngine eng;
public:
	ProcessTracer();
	
	ProcessTracer(int argc, TCHAR **argv);
	
	void run();

	void OnCreateProcess(DWORD processId, DWORD threadId, CREATE_PROCESS_DEBUG_INFO const & createProcess);

	void OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess);

	void OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread);

	void OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread);

	void OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll);

	void OnUnloadDll(UNLOAD_DLL_DEBUG_INFO const & unloadDll);

	void OnOutputDebugString(OUTPUT_DEBUG_STRING_INFO const & debugString);

	void OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception);

	void MyCreateProcess(int argc, TCHAR ** begin);

	static bool IsExistArgv(int argc);
};