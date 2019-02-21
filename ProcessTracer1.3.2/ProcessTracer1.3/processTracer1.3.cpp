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
ProcessTracer class ���Ǻκ�
������ϴµ� �־ �ٽ� �Լ� �� ���� ����

����� ��ɾ� �Ű������� ���μ����� ����
����� �̺�Ʈ�� �̿��Ͽ� ����� �ǽ�
�̺�Ʈ�� ����ü ����
���μ��� ���� , ������ ���� , Load File,Dll ���� , ���� ȣ�� ���� ��� ����
���� �߻� �� �߰� �������� ���
���μ��� ���� �� main ����
*/

#include <windows.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "ProcessTracer.h" 
#include <atlstr.h>

/** Simple process tracer */
/////////////////////////////////////////////////////////////////////////////////////////////////
/*������ 
ȯ�溯������*/
ProcessTracer::ProcessTracer()
{
	_putenv("_NO_DEBUG_HEAP=1");
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*������
ȯ�溯������
command line parameter�� �Ű������� MyCreateProcessȣ��*/
ProcessTracer::ProcessTracer(int argc, TCHAR **argv)
{
	_putenv("_NO_DEBUG_HEAP=1");
	this->MyCreateProcess(argc, argv);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : ����� �̺�Ʈ ��� => �̺�Ʈ�� ���� �Լ� ���� => ���μ��� ���� �̺�Ʈ(EXIT_PROCESS_DEBUG_EVENT) �� ����
���μ��� ����� bool completed = true , while (!completed) �ݺ��� �������´�.
���� ����� �̺�Ʈ 2�� �߻� ��  ����ó�� �� ���� => bool attached�� Ȱ��
��ȯ : void
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
			//continueFlag�� ������� ����
		{
			throw std::runtime_error("Error continuing debug event");
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : ���μ��� ����
SimpleSymbolEngine class object �ʱ�ȭ
�ɺ����̺� �ε� / ���μ����� ����������ּ�(lpStartAddress)���
��ȯ : void
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
��� : ���� Trace ȣ�� / �����ڵ����  0�̸� ����
��ȯ : void
*/
void ProcessTracer::OnExitProcess(DWORD threadId, EXIT_PROCESS_DEBUG_INFO const & exitProcess)
{
	std::cout << "EXIT PROCESS " << exitProcess.dwExitCode << std::endl;
	eng.stackTrace(threadHandles[threadId], std::cout);
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : ������ ���� , ������ �ּ� ���
��ȯ : void
*/
void ProcessTracer::OnCreateThread(DWORD threadId, CREATE_THREAD_DEBUG_INFO const & createThread)
{
	std::cout << "CREATE THREAD " << threadId << " at " << eng.addressToString(createThread.lpStartAddress) << std::endl;
	threadHandles[threadId] = createThread.hThread;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : ������ ���� / ���� ������ ���� ȣ�� /  �����ڵ� 0 �̸� ���� /
��ȯ : void
*/
void ProcessTracer::OnExitThread(DWORD threadId, EXIT_THREAD_DEBUG_INFO const & exitThread)
{
	std::cout << "EXIT THREAD " << threadId << ": " << exitThread.dwExitCode << std::endl;
	eng.stackTrace(threadHandles[threadId], std::cout);
	threadHandles.erase(threadId);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : DLL Load �� �ּҿ� �ִ� fileName���
��ȯ : void
*/
void ProcessTracer::OnLoadDll(LOAD_DLL_DEBUG_INFO const & loadDll)
{
	void *pString = 0;
	ReadProcessMemory(hProcess, loadDll.lpImageName, &pString, sizeof(pString), 0);
	std::string const fileName(eng.getString(pString, loadDll.fUnicode, MAX_PATH));
	/*lpImageName => hFile �ּ�
	loadDll.fUnicode => �����ڵ� ����
	���ڿ����� �˻�
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
��� : DLL UnLoad �� Address ���
��ȯ : void
*/
void ProcessTracer::OnUnloadDll(UNLOAD_DLL_DEBUG_INFO const & unloadDll)
{
	std::cout << "UNLOAD DLL " << unloadDll.lpBaseOfDll << std::endl;
	eng.unloadModule(unloadDll.lpBaseOfDll);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : ����� ��Ʈ�� ���
��ȯ : void
*/
void ProcessTracer::OnOutputDebugString(OUTPUT_DEBUG_STRING_INFO const & debugString)
{
	std::string const output(eng.getString(debugString.lpDebugStringData,
		debugString.fUnicode,//�����ڵ忩��
		debugString.nDebugStringLength));
	std::cout << "OUTPUT DEBUG STRING: " << output << std::endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/*
��� : �����ڵ� + �ּ� ��� ,  ���� ���� �߰� ��� 
���� Trace ȣ��(�������� ȣ��) , firstchance - ������ ���ܹ߻� ���� ��Ÿ��.
��ȯ : void
*/
void ProcessTracer::OnException(DWORD threadId, DWORD firstChance, EXCEPTION_RECORD const & exception)
{
	std::cout << "EXCEPTION 0x" << std::hex << exception.ExceptionCode << std::dec;
	std::cout << " at " << eng.addressToString(exception.ExceptionAddress);
	if (firstChance)
	{
		if (exception.NumberParameters)
			//NumberParameters - �������� ����
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
��� : �Ű������� command line parameter�� ���� ��Ʈ�� �ּҿ� ������ ��Ƽ�� ���� �ּҸ� �޴´�.
CString cmdLine�� command line parameter copy
command line parameter�� command �� �Ͽ� Process ����. 
��ȯ : void
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
							 //strchr function ã�����ϴ� ���� ������  return NULL
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
��ȯ : bool
��� : command line parameter ���翩��
�����ϸ� true , ������ false
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