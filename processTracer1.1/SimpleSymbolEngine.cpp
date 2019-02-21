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
심볼관련 함수 및 변수 정의
심볼 LOAD UNLOAD 하기위해서 초기화(init) 필수
*/
//심볼 테이블은 컴파일러 또는 인터프리터 같은 언어 변환기에서 사용되는 데이터 구조이다.
//쉽게말해 데이터구조의 일종이다.
#define _CRT_SECURE_NO_WARNINGS

#include "SimpleSymbolEngine.h"
#include <windows.h>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <vector>
#include <dbghelp.h>

#pragma comment( lib, "dbghelp" )
//라이브러리 추가
static char const szRCSID[] = "$Id: SimpleSymbolEngine.cpp 88 2011-11-19 14:10:18Z Roger $";

namespace
{
	// Helper function to read up to maxSize bytes from address in target process
	// into the supplied buffer.
	// Returns number of bytes actually read.
	/*
	반환 : SIZE_T - copy 스트링 길이
	기능 : address에 있는 String을 buffer로 copy.
	ReadProcessMemory함수를 사용 , ReadProcessMemory함수가 NULL 반환 시(read가 허용되지 않은 프로세스 access시  NULL 반환) 예외처리
	*/
	SIZE_T ReadPartialProcessMemory(HANDLE hProcess, LPCVOID address, LPVOID buffer, SIZE_T minSize, SIZE_T maxSize)
	{
		SIZE_T length = maxSize;
		while (length >= minSize)
		{
			if (ReadProcessMemory(hProcess, address, buffer, length, 0))
				/*read 성공 시 길이 반환  
				read가 허용되지 않은 프로세스 access시  NULL 반환*/
			{
				return length;
			}
			
			length--;

			static SYSTEM_INFO SystemInfo;
			static BOOL b = (GetSystemInfo(&SystemInfo), TRUE);
			SIZE_T pageOffset = ((ULONG_PTR)address + length) % SystemInfo.dwPageSize;
			/*(ULONG_PTR)address - 주소를 정수형으로 형변환*/
			/*SystemInfo.dwPageSize
			read가 허용되지 않은 프로세스 access시  NULL 반환
			초과되지 않도록 나머지 연산자를 이용한다.
			페이지 프레임 크기 - 4바이트 = 4096비트 = 0x400
			결과적으로 이진수 밑에 10자리 pageOffset에 save*/
			if (pageOffset > length)
				break;
			length -= pageOffset;
		}
		return 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 심볼 관련 옵션 2개 설정한다.
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
	2개 모드로 setting*/
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 심볼 초기화
1. 프로세스 핸들값
2. SymInitialize 함수로 Symbol 핸들 초기화
*/
void SimpleSymbolEngine::init(HANDLE hProcess)
{
	
	this->hProcess = hProcess;
	::SymInitialize(hProcess, 0, false);
	//Initializes the symbol handler for a process
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 프로세스 핸들정보를 이용하여 심볼 리소스 clean
*/
SimpleSymbolEngine::~SimpleSymbolEngine()
{
	::SymCleanup(hProcess);
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : string
기능 : address에 있는 심볼 정보 string으로 변환
SymFromAddr함수를 사용
파일로부터 변위(displacement)까지 기록
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
		/*uDisplacement	기준점에서의 변위*/
		/*psym - a pointer to a symbol_info structure that provides information about the symbol*/
	{
		oss << " " << pSym->Name;
		if (uDisplacement != 0)
		{
			LONG_PTR displacement = static_cast<LONG_PTR>(uDisplacement);
			//형변환 8byte -> 4byte

			if (displacement < 0)
				//abs(절댓값)처리
				oss << " - " << -displacement;
			else
				oss << " + " << displacement;
		}
	}

	// Finally any file/line number
	IMAGEHLP_LINE64 lineInfo = { sizeof(lineInfo) };
	DWORD dwDisplacement(0);
	if (SymGetLineFromAddr64(hProcess, reinterpret_cast<ULONG_PTR>(address), &dwDisplacement, &lineInfo))
		//빌드 경로 가져오기
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
반환 : void형
기능 : 심볼 테이블 LOAD
image file 및 address 필요
*/
void SimpleSymbolEngine::loadModule(HANDLE hFile, PVOID baseAddress, std::string const & fileName)
{
	::SymLoadModule64(hProcess, hFile, const_cast<char*>(fileName.c_str()), 0, reinterpret_cast<ULONG_PTR>(baseAddress), 0);
	//symbolTable load 
	//SymInitialize(초기화) 되어 있어야함.
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 심볼테이블 UnLoad
Image base address 필요
*/
void SimpleSymbolEngine::unloadModule(PVOID baseAddress)
{
	::SymUnloadModule64(hProcess, reinterpret_cast<ULONG_PTR>(baseAddress));
	//symbolTable Unload 
	//SymInitialize 되어 있어야함.
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 스택 호출을 통해 BasePointer(BP)(page Frame) , Instruction Register(IR)(Code Address) 주소 출력
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
	//x86버전으로 초기화
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
	/*stackFrame.AddrPC.Mode 주소 해석기
	16bit-->16bit , 16bit-->32bit , real-mode , flat-mode
	real-mode  => 가상모드 (seagment)

	AddrModeFlat mode
	Flat addressing. This is the only addressing mode supported by the library.*/

	stackFrame.AddrFrame.Offset = context.Ebp;
	//base 포인터
	stackFrame.AddrFrame.Mode = AddrModeFlat;

	stackFrame.AddrStack.Offset = context.Esp;
	//stack 포인터
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
	loop방지를 위해서 초기화*/
	os << "  Frame       Code address\n";
	while (::StackWalk64(machineType, hProcess, hThread,
		&stackFrame, pContext,
		/*stackFrame
		This structure receives information for the next frame, if the function call succeeds*/
		0, ::SymFunctionTableAccess64, ::SymGetModuleBase64, 0))
		/*stack trace를 얻는다.
		stack trace - 에러의 호출 스택*/
	{
		if (stackFrame.AddrPC.Offset == 0)
			/*stace Frame.AddrPC.Offset - 현재 스택이아닌 다음스택 정보를 가지고있다.
			NULL이면 top위치에 도달했음을 알 수 있다.*/
		{
			os << "Null address\n";
			break;
		}
		PVOID frame = reinterpret_cast<PVOID>(stackFrame.AddrFrame.Offset);
		//스택 프레임주소 = basePointer
		PVOID pc = reinterpret_cast<PVOID>(stackFrame.AddrPC.Offset);
	
		/*context.Eip(Instrution Register) 와 pc(Program Counter)의 주소는 같다.
		밑에 줄 같은 값 두번 출력된다.
		os << reinterpret_cast<PVOID>(context.Eip) << "    " << pc << std::endl;*/
		os << "  0x" << frame << "  " << addressToString(pc) << "\n";
		//Base Pointer 출력 - Instrution Regiser addrress출력 + 위치(변위(disaplcement)포함)
		if (lastBp >= stackFrame.AddrFrame.Offset)
		{
			os << "Stack frame out of sequence...\n";
			break;
		}
		lastBp = stackFrame.AddrFrame.Offset;
		//스택 주소 저장 (base poiter)
	}
	os.flush();
	//buffer empty work(버퍼비우기)
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : string(vertor시작주소 반환)
기능 : 해당 주소(address)에 있는 string을 얻는다.
WBCS 인 경우 MBCS로 변환
*/
std::string SimpleSymbolEngine::getString(PVOID address, BOOL unicode, DWORD maxStringLength)
{
	if (unicode)
	{
		std::vector<wchar_t> chVector(maxStringLength + 1);
		ReadPartialProcessMemory(hProcess, address, &chVector[0], sizeof(wchar_t), maxStringLength * sizeof(wchar_t));

		size_t const wcLen = wcstombs(0, &chVector[0], 0);
		/*1번쨰 매개변수 NULL이면
		wclen = 출력 문자열에 쓰여진 바이트의 숫자(를 반환)*/
		/*wcstombs
		유니코드 문자(WBCS) --> MBCS형태로 변환*/
		if (wcLen == (size_t)-1)
			//와이드 문자(유니코드)와 부딪혔을 떄 -1이다.
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
		//ReadProcessMemory NULL반환(inaccessible address 에 access)인 경우 예외 처리한다.
		return &chVector[0];
	}
}
