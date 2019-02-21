/*
NAME
ProcessTracer

DESCRIPTION
About the simplest debugger which is useful!

COPYRIGHT
Copyright (C) 2011 by Roger Orr <rogero@howzatt.demon.co.uk>

This software is distributed in the hope that it will be useful, but
without WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

Permission is granted to anyone to make or distribute verbatim
copies of this software provided that the copyright notice and
this permission notice are preserved, and that the distributor
grants the recipent permission for further distribution as permitted
by this notice.

Comments and suggestions are always welcome.
Please report bugs to rogero@howzatt.demon.co.uk.
*/
/*

****berfore Using this program , Set ****
1. 속성[property] - 일반[general] -  문자집합[character Set] - 멀티문자집합[Use Multi-Byte]
2. 프로젝트 생성 - ErrorSource.cpp , NormalSource.cpp 기존 파일 추가 
2개의 파일은 processTrace1.1안에 있습니다. 
3. commandline parameter(사용자 매개변수)설정
속성[property] - 디버깅[debugging] - 명령인수[command arguments]
(프로젝트 이름).exe   해당실행파일 디버깅 실시합니다.

사용자 명령어 매개변수로 프로세스를 생성
디버깅 이벤트를 이용하여 디버깅 실시
이벤트별 구조체 존재
프로세스 정보 , 스레드 정보 , Load File,Dll 정보 , 스택 호출 정보 등등 제공
예외 발생 시 추가 예외정보 출력
프로세스 종료 시 main 종료
*/

static char const szRCSID[] = "$Id: ProcessTracer.cpp 84 2011-11-13 00:29:15Z Roger $";

#ifdef _M_X64
#include <ntstatus.h>
#define WIN32_NO_STATUS
#endif // _M_X64

#include <windows.h>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include "SimpleSymbolEngine.h"

/** Simple process tracer */
/*
ProcessTracer class 선언부
디버깅하는데 있어서 핵심 함수 및 변수 포함
*/
class ProcessTracer
{
private:
	HANDLE hProcess;
	std::map<DWORD, HANDLE> threadHandles;
	/*Hash자료구조 STL
	map<key , value>*/
	SimpleSymbolEngine eng;
public:
	ProcessTracer()
	{
		_putenv("_NO_DEBUG_HEAP=1");
		//환경변수 지정
	}
	ProcessTracer(int argc, char **argv)
	{
		_putenv("_NO_DEBUG_HEAP=1");
		//환경변수 지정
		this->MyCreateProcess(argc, argv);
	}
	/** Run the debug loop */
	void run();

	/** Process has been created */
	void OnCreateProcess(DWORD processId, DWORD threadId, CREATE_PROCESS_DEBUG_INFO const & createProcess);

	/** Process has exited */
	void OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess);

	/** Thread has been created */
	void OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread);

	/** Thread has exited */
	void OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread);

	/** DLL has been loaded */
	void OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll);

	/** DLL has been unloaded */
	void OnUnloadDll(UNLOAD_DLL_DEBUG_INFO const & unloadDll);

	/** OutputDebugString has been called */
	void OnOutputDebugString(OUTPUT_DEBUG_STRING_INFO const & debugString);

	/** An exception has occurred */
	void OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception);
	/** Process has been created in commandline parameter*/
	void MyCreateProcess(int argc, char ** begin);
	/** If commandline parameter exist , {return true} else {return flase}*/
	static bool IsExistArgv(int argc);
};

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 디버깅 이벤트 사용 - 이벤트에 따라 함수 실행 - 프로세스 종료 이벤트(EXIT_PROCESS_DEBUG_EVENT) 시 종료
예외 디버그 이벤트 2번 발생 시  예외처리 후 종료
반환 : void
*/
void ProcessTracer::run()
{
	bool completed = false;
	//프로세스 종료시 true
	bool attached = false;
	//EXCEPTION_DEBUG_EVENT 발생 시 true
	//EXCEPTION_DEBUG_EVENT 2번 발생 시 예외던진다.

	while (!completed)
	{
		DEBUG_EVENT DebugEvent;
		//디버그 이벤트 선언

		if (!WaitForDebugEvent(&DebugEvent, INFINITE))
		{
			throw std::runtime_error("Debug loop aborted");
		}

		DWORD continueFlag = DBG_CONTINUE;
		switch (DebugEvent.dwDebugEventCode)
			//이벤트 종류
		{
		case CREATE_PROCESS_DEBUG_EVENT:
			OnCreateProcess(DebugEvent.dwProcessId, DebugEvent.dwThreadId, DebugEvent.u.CreateProcessInfo);
			break;

		case EXIT_PROCESS_DEBUG_EVENT:
			OnExitProcess(DebugEvent.dwThreadId, DebugEvent.u.ExitProcess);
			completed = true;
			break;

		case CREATE_THREAD_DEBUG_EVENT:
			OnCreateThread(DebugEvent.dwThreadId, DebugEvent.u.CreateThread);
			break;

		case EXIT_THREAD_DEBUG_EVENT:
			OnExitThread(DebugEvent.dwThreadId, DebugEvent.u.ExitThread);
			break;

		case LOAD_DLL_DEBUG_EVENT:
			OnLoadDll(DebugEvent.u.LoadDll);
			break;

		case UNLOAD_DLL_DEBUG_EVENT:
			OnUnloadDll(DebugEvent.u.UnloadDll);
			break;

		case OUTPUT_DEBUG_STRING_EVENT:
			OnOutputDebugString(DebugEvent.u.DebugString);
			break;

		case EXCEPTION_DEBUG_EVENT:
			if (!attached)
			{
				// First exception is special
				attached = true;
			}
#ifdef _M_X64
			else if (DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == STATUS_WX86_BREAKPOINT)
			{
				std::cout << "WOW64 initialised" << std::endl;
			}
#endif // _M_X64
			else
			{
				OnException(DebugEvent.dwThreadId, DebugEvent.u.Exception.dwFirstChance, DebugEvent.u.Exception.ExceptionRecord);
				/*예외 관련 정보 출력
				예외발생지점주소 , 코드 등등*/
				continueFlag = (DWORD)DBG_EXCEPTION_NOT_HANDLED;
				//DBG_EXCEPTION_NOT_HANDLED  접근 위반 핸들러에 있는 상수다.
			}
			break;

		default:
			std::cerr << "Unexpected debug event: " << DebugEvent.dwDebugEventCode << std::endl;
		}//switch문 끝

		if (!ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, continueFlag))
			//첫번째 예외 발생 시(attached = flase)디버그 이벤트 계속 진행  
			//두번쨰 예외 발생 시 예외 던진다.(continueFlag = EXCEPTION_DEBUG_EVENT) 
		{
			throw std::runtime_error("Error continuing debug event");
		}
	}//while문의 끝
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 프로세스 생성 - 스레드핸들추가 , 프로세스핸들 추가 , 심볼테이블 로드 / 프로세스 주소 출력
반환 : void
*/
void ProcessTracer::OnCreateProcess(DWORD processId, DWORD threadId, CREATE_PROCESS_DEBUG_INFO const & createProcess)
/*
lpStartAddress
A pointer to the starting address of the thread.
This value may only be an approximation(근사치) of the thread's starting address,
because any application with appropriate access to the thread can change the thread's context by using the SetThreadContext function.
*/
{
	hProcess = createProcess.hProcess;
	//프로세스 핸들 추가
	threadHandles[threadId] = createProcess.hThread;
	//스레드 핸들 추가
	eng.init(hProcess);
	/*SimpleSymbolEngine class object 초기화
	심볼 테이블 초기화*/
	eng.loadModule(createProcess.hFile, createProcess.lpBaseOfImage, std::string());
	//프로세스 심볼테이블 로드
	std::cout << "CREATE PROCESS " << processId << " at " << eng.addressToString(createProcess.lpStartAddress) << std::endl;
	//프로세스 시작주소 출력
	if (createProcess.hFile)
	{
		CloseHandle(createProcess.hFile);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 스택 Trace호출 후 프로세스 종료 /  종료코드 0이면 정상
스택 Trace 호출하기 위해 threadId를 사용한다.
반환 : void
*/
void ProcessTracer::OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess)
{
	std::cout << "EXIT PROCESS " << exitProcess.dwExitCode << std::endl;
	//프로세스 종료 코드 출력
	eng.stackTrace(threadHandles[threadId], std::cout);
	/*onExitThread함수와 달리 threadhandles.erase(threadid) 가 없는 이유
	completed가 true되면서 그냥 종료되기 때문에 */
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 스레드 생성 , 스레드 주소 출력
반환 : void
*/
void ProcessTracer::OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread)
{
	std::cout << "CREATE THREAD " << threadId << " at " << eng.addressToString(createThread.lpStartAddress) << std::endl;
	//스레드 시작주소 출력
	threadHandles[threadId] = createThread.hThread;
	//스레드 추가
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 스레드 종료 / 종료코드 0 이면 정상
반환 : void
*/
void ProcessTracer::OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread)
{
	std::cout << "EXIT THREAD " << threadId << ": " << exitThread.dwExitCode << std::endl;
	//스레드 종료 코드 출력
	eng.stackTrace(threadHandles[threadId], std::cout);
	//에러 스택 호출
	threadHandles.erase(threadId);
	//스레드 지운다
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : DLL Load 및 (Address + 주소에 있는fileName)출력
반환 : void
*/
void ProcessTracer::OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll)
{
	void *pString = 0;
	ReadProcessMemory(hProcess, loadDll.lpImageName, &pString, sizeof(pString), 0);
	/*&pString
	이중포인터 사용*/
	/* loadDll.lpImageName
	A pointer to the file name associated with hFile
	may contain the address of a string pointer in the address space of the process being debugged*/

	std::string const fileName(eng.getString(pString, loadDll.fUnicode, MAX_PATH));
	/*loadDll.fUnicode => 유니코드 여부
	최대길이 검사
	fileName 벡터 초기화
	access range check*/
	eng.loadModule(loadDll.hFile, loadDll.lpBaseOfDll, fileName);
	std::cout << "LOAD DLL " << loadDll.lpBaseOfDll << " " << fileName << std::endl;
	//Loading  DLL base address 출력
	if (loadDll.hFile)
	{
		CloseHandle(loadDll.hFile);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : DLL UnLoad 및 Address 출력
반환 : void
*/
void ProcessTracer::OnUnloadDll(UNLOAD_DLL_DEBUG_INFO const & unloadDll)
{
	std::cout << "UNLOAD DLL " << unloadDll.lpBaseOfDll << std::endl;
	//unLoading 위한 DLL base address 출력
	eng.unloadModule(unloadDll.lpBaseOfDll);
	//DLL unLoading
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 디버깅 스트링 출력
반환 : void
*/
void ProcessTracer::OnOutputDebugString(OUTPUT_DEBUG_STRING_INFO const & debugString)
{
	std::string const output(eng.getString(debugString.lpDebugStringData,
		debugString.fUnicode,//유니코드여부
		debugString.nDebugStringLength));
	//debugString.lpDebugStringData 의 스트링 길이
	//디버그 스트링 출력
	std::cout << "OUTPUT DEBUG STRING: " << output << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 예외 정보 추가 출력 , 디버깅 이벤트 때 사용 , 스택 Trace 호출(에러스택 호출)
스택호출 시 threadId사용
firstchance - 이전에 예외발생 여부 나타냄.
이전예외발생 X --> 예외코드 , 예외발생주소만 출력
반환 : void
*/
void ProcessTracer::OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception)
{
	std::cout << "EXCEPTION 0x" << std::hex << exception.ExceptionCode << std::dec;
	//16진수 모드 -- 예외 코드 출력  -- 10진수 모드 되돌림.
	std::cout << " at " << eng.addressToString(exception.ExceptionAddress);
	//예외 발생 주소 출력
	if (firstChance)
	{
		if (exception.NumberParameters)
			// 예외관련 정보 매개변수 개수 - ExceptionInformation의 인덱스 값
		{
			std::cout << "\n  Parameters:";
			for (DWORD idx = 0; idx != exception.NumberParameters; ++idx)
			{
				std::cout << " " << exception.ExceptionInformation[idx];
			}
		}
		//추가적인 예외 정보 출력
		std::cout << std::endl;

		eng.stackTrace(threadHandles[threadId], std::cout);
		//에러 스택 호출
	}
	else
	{
		std::cout << " (last chance)" << std::endl;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 매개변수로 command line parameter의 begin string 주소와 end string next 주소를 받는다.
command line parameter를 command 로 하여 Process 생성.
반환 : void
*/
void ProcessTracer::MyCreateProcess(int argc, char ** begin)
{
	++begin;
	--argc;
	char** end = begin + argc;
	std::string cmdLine;
	for (char **it = begin; it != end; ++it)
	{
		if (!cmdLine.empty()) cmdLine += ' ';

		if (strchr(*it, ' '))//== true
							 //strchr function 찾고자하는 문자 없으면  return NULL
		{
			cmdLine += '"';
			cmdLine += *it;
			cmdLine += '"';
		}
		else
		{
			cmdLine += *it;
		}
	}
	//command Argument --> cmdLine에 copy
	//프로세스 생성시 명령어로 사용
	STARTUPINFO startupInfo = { sizeof(startupInfo) };
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOWNORMAL; // Assist GUI programs
											 //si초기화
	PROCESS_INFORMATION ProcessInformation = { 0 };
	//pi초기화

	if (!CreateProcess(0, const_cast<char*>(cmdLine.c_str()),
		0, 0, true,
		DEBUG_ONLY_THIS_PROCESS,
		0, 0, &startupInfo, &ProcessInformation))
		//명령 매개변수를 통한 프로세스 생성
	{
		std::ostringstream oss;
		oss << GetLastError();
		throw std::runtime_error(std::string("Unable to start ") + *begin + ": " + oss.str());
	}

	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
bool ProcessTracer::IsExistArgv(int argc)
{
	if (argc <= 1)
	{
		printf("Syntax: ProcessTracer command_line\n");
		return false;
	}
	return true;
}
/*
기능 : main - 프로세스 생성 , 디버깅 , 예외처리
command line parameter 사용
반환 : int
*/
int main(int argc, char **argv)
{
	if (!ProcessTracer::IsExistArgv(argc))
		return 1;
	/*
	ProcessTracer* handler = new ProcessTracer;
	handler->MyCreateProcess(argc, argv);
	*/
	try
	{
		ProcessTracer* handler = new ProcessTracer(argc, argv);
		handler->run();
	}
	catch (std::exception &ex)
	{
		std::cerr << "Unexpected exception: " << ex.what() << std::endl;
		return 1;
	}
	return 0;
}













//#ifdef __MINGW32__
//#include <_mingw_mac.h>
//#endif
//
//#ifdef _M_X64
//#include <ntstatus.h>
//
//#define WIN32_NO_STATUS
//#endif // _M_X64
//
//#include <stdio.h>
//
//#include <windows.h>
//
//#include <iostream>
//#include <map>
//#include <sstream>
//#include <stdexcept>
//
//#include "SimpleSymbolEngine.h"
//
///** Simple process tracer */
//class ProcessTracer
//{
//private:
//	HANDLE hProcess;
//	std::map<DWORD, HANDLE> threadHandles;
//	SimpleSymbolEngine eng;
//
//public:
//	/** Run the debug loop */
//	void run();
//
//	/** Process has been created */
//	void OnCreateProcess(DWORD processId, DWORD threadId, CREATE_PROCESS_DEBUG_INFO const & createProcess);
//
//	/** Process has exited */
//	void OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess);
//
//	/** Thread has been created */
//	void OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread);
//
//	/** Thread has exited */
//	void OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread);
//
//	/** DLL has been loaded */
//	void OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll);
//
//	/** DLL has been unloaded */
//	void OnUnloadDll(UNLOAD_DLL_DEBUG_INFO const & unloadDll);
//
//	/** OutputDebugString has been called */
//	void OnOutputDebugString(OUTPUT_DEBUG_STRING_INFO const & debugString);
//
//	/** An exception has occurred */
//	void OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception);
//};
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::run()
//{
//	bool completed = false;
//	bool attached = false;
//
//	while (!completed)
//	{
//		DEBUG_EVENT DebugEvent;
//		if (!WaitForDebugEvent(&DebugEvent, INFINITE))
//		{
//			throw std::runtime_error("Debug loop aborted");
//		}
//
//		DWORD continueFlag = DBG_CONTINUE;
//		switch (DebugEvent.dwDebugEventCode)
//		{
//		case CREATE_PROCESS_DEBUG_EVENT:
//			OnCreateProcess(DebugEvent.dwProcessId, DebugEvent.dwThreadId, DebugEvent.u.CreateProcessInfo);
//			break;
//
//		case EXIT_PROCESS_DEBUG_EVENT:
//			OnExitProcess(DebugEvent.dwThreadId, DebugEvent.u.ExitProcess);
//			completed = true;
//			break;
//
//		case CREATE_THREAD_DEBUG_EVENT:
//			OnCreateThread(DebugEvent.dwThreadId, DebugEvent.u.CreateThread);
//			break;
//
//		case EXIT_THREAD_DEBUG_EVENT:
//			OnExitThread(DebugEvent.dwThreadId, DebugEvent.u.ExitThread);
//			break;
//
//		case LOAD_DLL_DEBUG_EVENT:
//			OnLoadDll(DebugEvent.u.LoadDll);
//			break;
//
//		case UNLOAD_DLL_DEBUG_EVENT:
//			OnUnloadDll(DebugEvent.u.UnloadDll);
//			break;
//
//		case OUTPUT_DEBUG_STRING_EVENT:
//			OnOutputDebugString(DebugEvent.u.DebugString);
//			break;
//
//		case EXCEPTION_DEBUG_EVENT:
//			if (!attached)
//			{
//				// First exception is special
//				attached = true;
//			}
//#ifdef _M_X64
//			else if (DebugEvent.u.Exception.ExceptionRecord.ExceptionCode == STATUS_WX86_BREAKPOINT)
//			{
//				std::cout << "WOW64 initialised" << std::endl;
//			}
//#endif // _M_X64
//			else
//			{
//				OnException(DebugEvent.dwThreadId, DebugEvent.u.Exception.dwFirstChance, DebugEvent.u.Exception.ExceptionRecord);
//				continueFlag = (DWORD)DBG_EXCEPTION_NOT_HANDLED;
//			}
//			break;
//
//		default:
//			std::cerr << "Unexpected debug event: " << DebugEvent.dwDebugEventCode << std::endl;
//		}
//
//		if (!ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, continueFlag))
//		{
//			throw std::runtime_error("Error continuing debug event");
//		}
//	}
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnCreateProcess(DWORD processId, DWORD threadId, CREATE_PROCESS_DEBUG_INFO const & createProcess)
//{
//	hProcess = createProcess.hProcess;
//	threadHandles[threadId] = createProcess.hThread;
//	eng.init(hProcess);
//
//	eng.loadModule(createProcess.hFile, createProcess.lpBaseOfImage, std::string());
//
//	std::cout << "CREATE PROCESS " << processId << " at " << eng.addressToString((PVOID)createProcess.lpStartAddress) << std::endl;
//
//	if (createProcess.hFile)
//	{
//		CloseHandle(createProcess.hFile);
//	}
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess)
//{
//	std::cout << "EXIT PROCESS " << exitProcess.dwExitCode << std::endl;
//
//	eng.stackTrace(threadHandles[threadId], std::cout);
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread)
//{
//	std::cout << "CREATE THREAD " << threadId << " at " << eng.addressToString((PVOID)createThread.lpStartAddress) << std::endl;
//
//	threadHandles[threadId] = createThread.hThread;
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread)
//{
//	std::cout << "EXIT THREAD " << threadId << ": " << exitThread.dwExitCode << std::endl;
//
//	eng.stackTrace(threadHandles[threadId], std::cout);
//
//	threadHandles.erase(threadId);
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll)
//{
//	void *pString = 0;
//	ReadProcessMemory(hProcess, loadDll.lpImageName, &pString, sizeof(pString), 0);
//	std::string const fileName(eng.getString(pString, loadDll.fUnicode, MAX_PATH));
//
//	eng.loadModule(loadDll.hFile, loadDll.lpBaseOfDll, fileName);
//
//	std::cout << "LOAD DLL " << loadDll.lpBaseOfDll << " " << fileName << std::endl;
//
//	if (loadDll.hFile)
//	{
//		CloseHandle(loadDll.hFile);
//	}
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnUnloadDll(UNLOAD_DLL_DEBUG_INFO const & unloadDll)
//{
//	std::cout << "UNLOAD DLL " << unloadDll.lpBaseOfDll << std::endl;
//
//	eng.unloadModule(unloadDll.lpBaseOfDll);
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnOutputDebugString(OUTPUT_DEBUG_STRING_INFO const & debugString)
//{
//	std::string const output(eng.getString(debugString.lpDebugStringData,
//		debugString.fUnicode,
//		debugString.nDebugStringLength));
//
//	std::cout << "OUTPUT DEBUG STRING: " << output << std::endl;
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void ProcessTracer::OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception)
//{
//	std::cout << "EXCEPTION 0x" << std::hex << exception.ExceptionCode << std::dec;
//	std::cout << " at " << eng.addressToString(exception.ExceptionAddress);
//
//	if (firstChance)
//	{
//		if (exception.NumberParameters)
//		{
//			std::cout << "\n  Parameters:";
//			for (DWORD idx = 0; idx != exception.NumberParameters; ++idx)
//			{
//				std::cout << " " << exception.ExceptionInformation[idx];
//			}
//		}
//		std::cout << std::endl;
//
//		eng.stackTrace(threadHandles[threadId], std::cout);
//	}
//	else
//	{
//		std::cout << " (last chance)" << std::endl;
//	}
//}
//
///////////////////////////////////////////////////////////////////////////////////////////////////
//void CreateProcess(char ** begin, char ** end)
//{
//	std::string cmdLine;
//	for (char **it = begin; it != end; ++it)
//	{
//		if (!cmdLine.empty()) cmdLine += ' ';
//
//		if (strchr(*it, ' '))
//		{
//			cmdLine += '"';
//			cmdLine += *it;
//			cmdLine += '"';
//		}
//		else
//		{
//			cmdLine += *it;
//		}
//	}
//
//	STARTUPINFO startupInfo = { sizeof(startupInfo) };
//	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
//	startupInfo.wShowWindow = SW_SHOWNORMAL; // Assist GUI programs
//	PROCESS_INFORMATION ProcessInformation = { 0 };
//
//	if (!CreateProcess(0, const_cast<char*>(cmdLine.c_str()),
//		0, 0, true,
//		DEBUG_ONLY_THIS_PROCESS,
//		0, 0, &startupInfo, &ProcessInformation))
//	{
//		std::ostringstream oss;
//		oss << GetLastError();
//		throw std::runtime_error(std::string("Unable to start ") + *begin + ": " + oss.str());
//	}
//
//	CloseHandle(ProcessInformation.hProcess);
//	CloseHandle(ProcessInformation.hThread);
//}
//
//int main(int argc, char **argv)
//{
//	if (argc <= 1)
//	{
//		printf("Syntax: ProcessTracer command_line\n");
//		return 1;
//	}
//	++argv;
//	--argc;
//	// Use the normal heap manager
//	_putenv("_NO_DEBUG_HEAP=1");
//	try
//	{
//		CreateProcess(argv, argv + argc);
//		ProcessTracer().run();
//	}
//	catch (std::exception &ex)
//	{
//		std::cerr << "Unexpected exception: " << ex.what() << std::endl;
//		return 1;
//	}
//	return 0;
//}
//
