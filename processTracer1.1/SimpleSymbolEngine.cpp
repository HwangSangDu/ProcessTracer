/*
NAME
SimpleSymbolEngine

DESCRIPTION
Simple symbol engine functionality.
This is demonstration code only - it is non. thread-safe and single instance.

COPYRIGHT
Copyright (C) 2004, 2011 by Roger Orr <rogero@howzatt.demon.co.uk>

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
SimpleSymbolEngine.cpp
�ɺ����� �Լ� �� ���� ����
�ɺ� LOAD UNLOAD �ϱ����ؼ� �ʱ�ȭ(init) �ʼ�
*/
//�ɺ� ���̺��� �����Ϸ� �Ǵ� ���������� ���� ��� ��ȯ�⿡�� ���Ǵ� ������ �����̴�.
//���Ը��� �����ͱ����� �����̴�.
#define _CRT_SECURE_NO_WARNINGS

#include "SimpleSymbolEngine.h"
#include <windows.h>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <vector>
#include <dbghelp.h>

#pragma comment( lib, "dbghelp" )
//���̺귯�� �߰�
static char const szRCSID[] = "$Id: SimpleSymbolEngine.cpp 88 2011-11-19 14:10:18Z Roger $";

namespace
{
	// Helper function to read up to maxSize bytes from address in target process
	// into the supplied buffer.
	// Returns number of bytes actually read.
	/*
	��ȯ : SIZE_T - copy ��Ʈ�� ����
	��� : address�� �ִ� String�� buffer�� copy.
	ReadProcessMemory�Լ��� ��� , ReadProcessMemory�Լ��� NULL ��ȯ ��(read�� ������ ���� ���μ��� access��  NULL ��ȯ) ����ó��
	*/
	SIZE_T ReadPartialProcessMemory(HANDLE hProcess, LPCVOID address, LPVOID buffer, SIZE_T minSize, SIZE_T maxSize)
	{
		SIZE_T length = maxSize;
		while (length >= minSize)
		{
			if (ReadProcessMemory(hProcess, address, buffer, length, 0))
				/*read ���� �� ���� ��ȯ  
				read�� ������ ���� ���μ��� access��  NULL ��ȯ*/
			{
				return length;
			}
			
			length--;

			static SYSTEM_INFO SystemInfo;
			static BOOL b = (GetSystemInfo(&SystemInfo), TRUE);
			SIZE_T pageOffset = ((ULONG_PTR)address + length) % SystemInfo.dwPageSize;
			/*(ULONG_PTR)address - �ּҸ� ���������� ����ȯ*/
			/*SystemInfo.dwPageSize
			read�� ������ ���� ���μ��� access��  NULL ��ȯ
			�ʰ����� �ʵ��� ������ �����ڸ� �̿��Ѵ�.
			������ ������ ũ�� - 4����Ʈ = 4096��Ʈ = 0x400
			��������� ������ �ؿ� 10�ڸ� pageOffset�� save*/
			if (pageOffset > length)
				break;
			length -= pageOffset;
		}
		return 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : void
��� : �ɺ� ���� �ɼ� 2�� �����Ѵ�.
1. SYMOPT_LOAD_LINES
This symbol option allows line number information to be read from source files
2. SYMOPT_OMAP_FIND_NEAREST
there is no symbol at the expected location, this option causes the nearest symbol to be used instead
*/
SimpleSymbolEngine::SimpleSymbolEngine()
{
	DWORD dwOpts = SymGetOptions();
	/*SymGetOptions()
	The function returns the current options that have been set. Zero is a valid value and indicates that all options are turned off. */
	dwOpts |= SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST;

	SymSetOptions(dwOpts);
	/*symopt_load_lines | symopt_omap_find_nearest
	2�� ���� setting*/
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : void
��� : �ɺ� �ʱ�ȭ
1. ���μ��� �ڵ鰪
2. SymInitialize �Լ��� Symbol �ڵ� �ʱ�ȭ
*/
void SimpleSymbolEngine::init(HANDLE hProcess)
{
	
	this->hProcess = hProcess;
	::SymInitialize(hProcess, 0, false);
	//Initializes the symbol handler for a process
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : void
��� : ���μ��� �ڵ������� �̿��Ͽ� �ɺ� ���ҽ� clean
*/
SimpleSymbolEngine::~SimpleSymbolEngine()
{
	::SymCleanup(hProcess);
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : string
��� : address�� �ִ� �ɺ� ���� string���� ��ȯ
SymFromAddr�Լ��� ���
���Ϸκ��� ����(displacement)���� ���
*/
std::string SimpleSymbolEngine::addressToString(PVOID address)
{
	std::ostringstream oss;
	// First the raw address
	oss << "0x" << address;
	// Then symbol, if any
	struct
	{
		SYMBOL_INFO symInfo;
		char name[4 * 256];
	} SymInfo = { { sizeof(SymInfo.symInfo) }, "" };

	PSYMBOL_INFO pSym = &SymInfo.symInfo;
	pSym->MaxNameLen = sizeof(SymInfo.name);
	DWORD64 uDisplacement(0);
	if (SymFromAddr(hProcess, reinterpret_cast<ULONG_PTR>(address), &uDisplacement, pSym))
		//Retrieves symbol information for the specified address.
		/*uDisplacement	������������ ����*/
		/*psym - a pointer to a symbol_info structure that provides information about the symbol*/
	{
		oss << " " << pSym->Name;
		if (uDisplacement != 0)
		{
			LONG_PTR displacement = static_cast<LONG_PTR>(uDisplacement);
			//����ȯ 8byte -> 4byte

			if (displacement < 0)
				//abs(����)ó��
				oss << " - " << -displacement;
			else
				oss << " + " << displacement;
		}
	}

	// Finally any file/line number
	IMAGEHLP_LINE64 lineInfo = { sizeof(lineInfo) };
	DWORD dwDisplacement(0);
	if (SymGetLineFromAddr64(hProcess, reinterpret_cast<ULONG_PTR>(address), &dwDisplacement, &lineInfo))
		//���� ��� ��������
		/*SymGetLineFromAddr64
		Locates the source line for the specified address. */
	{
		oss << "   " << lineInfo.FileName << "(" << lineInfo.LineNumber << ")";
		if (dwDisplacement != 0)
		{
			oss << " + " << dwDisplacement << " byte" << (dwDisplacement == 1 ? "" : "s");
		}
	}
	return oss.str();
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : void��
��� : �ɺ� ���̺� LOAD
image file �� address �ʿ�
*/
void SimpleSymbolEngine::loadModule(HANDLE hFile, PVOID baseAddress, std::string const & fileName)
{
	::SymLoadModule64(hProcess, hFile, const_cast<char*>(fileName.c_str()), 0, reinterpret_cast<ULONG_PTR>(baseAddress), 0);
	//symbolTable load 
	//SymInitialize(�ʱ�ȭ) �Ǿ� �־����.
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : void
��� : �ɺ����̺� UnLoad
Image base address �ʿ�
*/
void SimpleSymbolEngine::unloadModule(PVOID baseAddress)
{
	::SymUnloadModule64(hProcess, reinterpret_cast<ULONG_PTR>(baseAddress));
	//symbolTable Unload 
	//SymInitialize �Ǿ� �־����.
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : void
��� : ���� ȣ���� ���� BasePointer(BP)(page Frame) , Instruction Register(IR)(Code Address) �ּ� ���
*/
void SimpleSymbolEngine::stackTrace(HANDLE hThread, std::ostream & os)

{
	CONTEXT context = { 0 };
	/*CONTEXT
	Contains processor-specific register data.
	The system uses CONTEXT structures to perform various internal operations*/
	PVOID pContext = &context;
	STACKFRAME64 stackFrame = { 0 };
#ifdef _M_IX86
	DWORD const machineType = IMAGE_FILE_MACHINE_I386;
	//x86�������� �ʱ�ȭ
	context.ContextFlags = CONTEXT_FULL;
	/* this section is specified/returned if context_debug_registers is
	set in contextflags. note that context_debug_registers is not
	included in context_full.*/

	GetThreadContext(hThread, &context);
	//Retrieves the context of the specified thread.
	stackFrame.AddrPC.Offset = context.Eip;
	//AddrPC - Adrress Program Counter
	//Eip - Instructin Register

	stackFrame.AddrPC.Mode = AddrModeFlat;
	/*stackFrame.AddrPC.Mode �ּ� �ؼ���
	16bit-->16bit , 16bit-->32bit , real-mode , flat-mode
	real-mode  => ������ (seagment)

	AddrModeFlat mode
	Flat addressing. This is the only addressing mode supported by the library.*/

	stackFrame.AddrFrame.Offset = context.Ebp;
	//base ������
	stackFrame.AddrFrame.Mode = AddrModeFlat;

	stackFrame.AddrStack.Offset = context.Esp;
	//stack ������
	stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
	DWORD machineType;

	BOOL bWow64(false);
	WOW64_CONTEXT wow64_context = { 0 };
	IsWow64Process(hProcess, &bWow64);
	if (bWow64)
	{
		machineType = IMAGE_FILE_MACHINE_I386;
		wow64_context.ContextFlags = WOW64_CONTEXT_FULL;
		Wow64GetThreadContext(hThread, &wow64_context);
		pContext = &wow64_context;
		stackFrame.AddrPC.Offset = wow64_context.Eip;
		stackFrame.AddrPC.Mode = AddrModeFlat;

		stackFrame.AddrFrame.Offset = wow64_context.Ebp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;

		stackFrame.AddrStack.Offset = wow64_context.Esp;
		stackFrame.AddrStack.Mode = AddrModeFlat;
	}
	else
	{
		machineType = IMAGE_FILE_MACHINE_AMD64;
		context.ContextFlags = CONTEXT_FULL;
		GetThreadContext(hThread, &context);

		stackFrame.AddrPC.Offset = context.Rip;
		stackFrame.AddrPC.Mode = AddrModeFlat;

		stackFrame.AddrFrame.Offset = context.Rbp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;

		stackFrame.AddrStack.Offset = context.Rsp;
		stackFrame.AddrStack.Mode = AddrModeFlat;
	}
#else
#error Unsupported target platform
#endif // _M_IX86

	DWORD64 lastBp = 0;
	/*BasePointer
	Prevent loops with optimised stackframes
	loop������ ���ؼ� �ʱ�ȭ*/
	os << "  Frame       Code address\n";
	while (::StackWalk64(machineType, hProcess, hThread,
		&stackFrame, pContext,
		/*stackFrame
		This structure receives information for the next frame, if the function call succeeds*/
		0, ::SymFunctionTableAccess64, ::SymGetModuleBase64, 0))
		/*stack trace�� ��´�.
		stack trace - ������ ȣ�� ����*/
	{
		if (stackFrame.AddrPC.Offset == 0)
			/*stace Frame.AddrPC.Offset - ���� �����̾ƴ� �������� ������ �������ִ�.
			NULL�̸� top��ġ�� ���������� �� �� �ִ�.*/
		{
			os << "Null address\n";
			break;
		}
		PVOID frame = reinterpret_cast<PVOID>(stackFrame.AddrFrame.Offset);
		//���� �������ּ� = basePointer
		PVOID pc = reinterpret_cast<PVOID>(stackFrame.AddrPC.Offset);
	
		/*context.Eip(Instrution Register) �� pc(Program Counter)�� �ּҴ� ����.
		�ؿ� �� ���� �� �ι� ��µȴ�.
		os << reinterpret_cast<PVOID>(context.Eip) << "    " << pc << std::endl;*/
		os << "  0x" << frame << "  " << addressToString(pc) << "\n";
		//Base Pointer ��� - Instrution Regiser addrress��� + ��ġ(����(disaplcement)����)
		if (lastBp >= stackFrame.AddrFrame.Offset)
		{
			os << "Stack frame out of sequence...\n";
			break;
		}
		lastBp = stackFrame.AddrFrame.Offset;
		//���� �ּ� ���� (base poiter)
	}
	os.flush();
	//buffer empty work(���ۺ���)
}

/////////////////////////////////////////////////////////////////////////////////////
/*
��ȯ : string(vertor�����ּ� ��ȯ)
��� : �ش� �ּ�(address)�� �ִ� string�� ��´�.
WBCS �� ��� MBCS�� ��ȯ
*/
std::string SimpleSymbolEngine::getString(PVOID address, BOOL unicode, DWORD maxStringLength)
{
	if (unicode)
	{
		std::vector<wchar_t> chVector(maxStringLength + 1);
		ReadPartialProcessMemory(hProcess, address, &chVector[0], sizeof(wchar_t), maxStringLength * sizeof(wchar_t));

		size_t const wcLen = wcstombs(0, &chVector[0], 0);
		/*1���� �Ű����� NULL�̸�
		wclen = ��� ���ڿ��� ������ ����Ʈ�� ����(�� ��ȯ)*/
		/*wcstombs
		�����ڵ� ����(WBCS) --> MBCS���·� ��ȯ*/
		if (wcLen == (size_t)-1)
			//���̵� ����(�����ڵ�)�� �ε����� �� -1�̴�.
		{
			return "invalid string";
		}
		else
		{
			std::vector<char> mbStr(wcLen + 1);
			wcstombs(&mbStr[0], &chVector[0], wcLen);
			//mbstr(MBCS) <-- chVector(WBCS) , (Byte)
			return &mbStr[0];
		}
	}
	else
	{
		std::vector<char> chVector(maxStringLength + 1);
		ReadPartialProcessMemory(hProcess, address, &chVector[0], 1, maxStringLength);
		//ReadProcessMemory NULL��ȯ(inaccessible address �� access)�� ��� ���� ó���Ѵ�.
		return &chVector[0];
	}
}
