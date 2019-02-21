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
ProcessTracer class 정의부분
디버깅하는데 있어서 핵심 함수 및 변수 포함

사용자 명령어 매개변수로 프로세스를 생성
디버깅 이벤트를 이용하여 디버깅 실시
이벤트별 구조체 존재
프로세스 정보 , 스레드 정보 , Load File,Dll 정보 , 스택 호출 정보 등등 제공
예외 발생 시 추가 예외정보 출력
프로세스 종료 시 main 종료
*/

#include <windows.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "ProcessTracer.h" 
#include <atlstr.h>

/** Simple process tracer */
/////////////////////////////////////////////////////////////////////////////////////////////////
/*생성자 
환경변수지정*/
ProcessTracer::ProcessTracer()
{
	_putenv("_NO_DEBUG_HEAP=1");
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*생성자
환경변수지정
command line parameter를 매개변수로 MyCreateProcess호출*/
ProcessTracer::ProcessTracer(int argc, TCHAR **argv)
{
	_putenv("_NO_DEBUG_HEAP=1");
	this->MyCreateProcess(argc, argv);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 디버깅 이벤트 사용 => 이벤트에 따라 함수 실행 => 프로세스 종료 이벤트(EXIT_PROCESS_DEBUG_EVENT) 시 종료
프로세스 종료시 bool completed = true , while (!completed) 반복문 빠져나온다.
예외 디버그 이벤트 2번 발생 시  예외처리 후 종료 => bool attached를 활용
반환 : void
*/
void ProcessTracer::run()
{
	bool completed = false;
	bool attached = false;
	while (!completed)
	{
		DEBUG_EVENT DebugEvent;
		if (!WaitForDebugEvent(&DebugEvent, INFINITE))
		{
			throw std::runtime_error("Debug loop aborted");
		}
		DWORD continueFlag = DBG_CONTINUE;
		switch (DebugEvent.dwDebugEventCode)
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
				continueFlag = (DWORD)DBG_EXCEPTION_NOT_HANDLED;
			}
			break;
		default:
			std::cerr << "Unexpected debug event: " << DebugEvent.dwDebugEventCode << std::endl;
		}
		if (!ContinueDebugEvent(DebugEvent.dwProcessId, DebugEvent.dwThreadId, continueFlag))
			//continueFlag의 상수값이 관건
		{
			throw std::runtime_error("Error continuing debug event");
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 프로세스 생성
SimpleSymbolEngine class object 초기화
심볼테이블 로드 / 프로세스의 스레드시작주소(lpStartAddress)출력
반환 : void
*/
void ProcessTracer::OnCreateProcess(DWORD processId, DWORD threadId, CREATE_PROCESS_DEBUG_INFO const & createProcess)
{
	hProcess = createProcess.hProcess;
	threadHandles[threadId] = createProcess.hThread;
	eng.init(hProcess);
	eng.loadModule(createProcess.hFile, createProcess.lpBaseOfImage, std::string());
	std::cout << "CREATE PROCESS " << processId << " at " << eng.addressToString(createProcess.lpStartAddress) << std::endl;
	
	if (createProcess.hFile)
	{
		CloseHandle(createProcess.hFile);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 스택 Trace 호출 / 종료코드출력  0이면 정상
반환 : void
*/
void ProcessTracer::OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess)
{
	std::cout << "EXIT PROCESS " << exitProcess.dwExitCode << std::endl;
	eng.stackTrace(threadHandles[threadId], std::cout);
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 스레드 생성 , 스레드 주소 출력
반환 : void
*/
void ProcessTracer::OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread)
{
	std::cout << "CREATE THREAD " << threadId << " at " << eng.addressToString(createThread.lpStartAddress) << std::endl;
	threadHandles[threadId] = createThread.hThread;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 스레드 종료 / 종료 스레드 스택 호출 /  종료코드 0 이면 정상 /
반환 : void
*/
void ProcessTracer::OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread)
{
	std::cout << "EXIT THREAD " << threadId << ": " << exitThread.dwExitCode << std::endl;
	eng.stackTrace(threadHandles[threadId], std::cout);
	threadHandles.erase(threadId);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : DLL Load 및 주소에 있는 fileName출력
반환 : void
*/
void ProcessTracer::OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll)
{
	void *pString = 0;
	ReadProcessMemory(hProcess, loadDll.lpImageName, &pString, sizeof(pString), 0);
	std::string const fileName(eng.getString(pString, loadDll.fUnicode, MAX_PATH));
	/*lpImageName => hFile 주소
	loadDll.fUnicode => 유니코드 여부
	문자열길이 검사
	access range check*/
	eng.loadModule(loadDll.hFile, loadDll.lpBaseOfDll, fileName);
	std::cout << "LOAD DLL " << loadDll.lpBaseOfDll << " " << fileName << std::endl;
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
	eng.unloadModule(unloadDll.lpBaseOfDll);
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
	std::cout << "OUTPUT DEBUG STRING: " << output << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 예외코드 + 주소 출력 ,  예외 정보 추가 출력 
스택 Trace 호출(에러스택 호출) , firstchance - 이전에 예외발생 여부 나타냄.
반환 : void
*/
void ProcessTracer::OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception)
{
	std::cout << "EXCEPTION 0x" << std::hex << exception.ExceptionCode << std::dec;
	std::cout << " at " << eng.addressToString(exception.ExceptionAddress);
	if (firstChance)
	{
		if (exception.NumberParameters)
			//NumberParameters - 예외정보 개수
		{
			std::cout << "\n  Parameters:";
			for (DWORD idx = 0; idx != exception.NumberParameters; ++idx)
			{
				std::cout << " " << exception.ExceptionInformation[idx];
			}
		}
		std::cout << std::endl;
		eng.stackTrace(threadHandles[threadId], std::cout);
	}
	else
	{
		std::cout << " (last chance)" << std::endl;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
기능 : 매개변수로 command line parameter의 시작 스트링 주소와 마지막 스티링 다음 주소를 받는다.
CString cmdLine에 command line parameter copy
command line parameter를 command 로 하여 Process 생성. 
반환 : void
*/
void ProcessTracer::MyCreateProcess(int argc, TCHAR ** begin)
{
	++begin;
	--argc;
	TCHAR** end = begin + argc;
	CString cmdLine;
	for (TCHAR **it = begin; it != end; ++it)
	{
		if (!cmdLine.IsEmpty()) cmdLine += ' ';

		if (_tcschr(*it, ' '))//== true
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

	STARTUPINFO startupInfo = { sizeof(startupInfo) };
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOWNORMAL; 
	// Assist GUI programs
	PROCESS_INFORMATION ProcessInformation = { 0 };

	if (!CreateProcess(0, const_cast<TCHAR *>(cmdLine.GetString()),
		0, 0, true,
		DEBUG_ONLY_THIS_PROCESS,
		0, 0, &startupInfo, &ProcessInformation))
	{
		std::ostringstream oss;
		oss << GetLastError();
#ifdef UNICODE
		size_t len = _tcslen(*begin) + 1;
		char *str = new char[len];
		wcstombs(str, *begin, len);
		//WBCS => MBCS
		throw std::runtime_error(std::string("Unable to start ") + str + ": " + oss.str());
		delete str;
#else
		throw std::runtime_error(std::string("Unable to start ") + *begin + ": " + oss.str());
#endif
	}
	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/*
반환 : bool
기능 : command line parameter 존재여부
존재하면 true , 없으면 false
*/
bool ProcessTracer::IsExistArgv(int argc)
{
	if (argc <= 1)
	{
		printf("Syntax: ProcessTracer command_line\n");
		return false;
	}
	return true;
}