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
*/
#include "SimpleSymbolEngine.h"


//dbghelp 라이브러리 추가
#pragma comment( lib, "dbghelp" )

static char const szRCSID[] = "$Id: SimpleSymbolEngine.cpp 88 2011-11-19 14:10:18Z Roger $";

namespace
{
	/*
	반환 : SIZE_T  => copy 스트링 길이 (읽어들인 스트링 길이)
	기능 :
	Helper function to read up to maxSize bytes from address in target process into the supplied buffer.
	Returns number of bytes actually read.
	ReadProcessMemory함수를 사용 , ReadProcessMemory함수가 NULL 반환 시(read가 허용되지 않은 프로세스 access시  NULL 반환) 예외처리

	*예외처리*
	SIZE_T pageOffset = ((ULONG_PTR)address + length) % SystemInfo.dwPageSize;
	length -= pageOffset;
	SystemInfo.dwPageSize = 페이지 프레임 크기 = 4바이트 = 4096비트 = 0x400
	NEXT 페이지(Frame)에 접근하지 않도록 빼준다.
	*/
	SIZE_T ReadPartialProcessMemory(HANDLE hProcess, LPCVOID address, LPVOID buffer, SIZE_T minSize, SIZE_T maxSize)
	{
		SIZE_T length = maxSize;
		while (length >= minSize)
		{
			if (ReadProcessMemory(hProcess, address, buffer, length, 0))
			{
				return length;
			}
			length--;
			static SYSTEM_INFO SystemInfo;
			static BOOL b = (GetSystemInfo(&SystemInfo), TRUE);
			SIZE_T pageOffset = ((ULONG_PTR)address + length) % SystemInfo.dwPageSize;

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
기능 :
SymGetOptions()를 활용 => 설정가능한 옵션을 return해준다.
심볼 관련 옵션 2개 설정한다.
1. SYMOPT_LOAD_LINES
This symbol option allows line number information to be read from source files
2. SYMOPT_OMAP_FIND_NEAREST
there is no symbol at the expected location, this option causes the nearest symbol to be used instead
*/
SimpleSymbolEngine::SimpleSymbolEngine()
{
	DWORD dwOpts = SymGetOptions();
	dwOpts |= SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST;
	SymSetOptions(dwOpts);
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
1. law 주소출력
std::ostringstream oss;
2. SymFromAddr함수 => 주소에 있는 심볼정보 기록
파일로부터의 변위(displacement)까지 psym에 기록
3.SymGetLineFromAddr64함수 => 주소에 있는 문자열 기록
파일로부터의 변위(displacement)를 lineInfo에 기록

변위(displacement)의 경우 문자열로 기록하기 위해 4바이트로 변환
static_cast<LONG_PTR>(uDisplacement)

절댓값처리도 해준다.
if (displacement < 0)
*/
std::string SimpleSymbolEngine::addressToString(PVOID address)
{
	std::ostringstream oss;

	oss << "0x" << address;

	struct
	{
		SYMBOL_INFO symInfo;
		char name[4 * 256];
	} SymInfo = { { sizeof(SymInfo.symInfo) }, "" };

	PSYMBOL_INFO pSym = &SymInfo.symInfo;
	pSym->MaxNameLen = sizeof(SymInfo.name);
	DWORD64 uDisplacement(0);
	if (SymFromAddr(hProcess, reinterpret_cast<ULONG_PTR>(address), &uDisplacement, pSym))
	{
		oss << " " << pSym->Name;
		if (uDisplacement != 0)
		{
			LONG_PTR displacement = static_cast<LONG_PTR>(uDisplacement);
			if (displacement < 0)
				oss << " - " << -displacement;
			else
				oss << " + " << displacement;
		}
	}
	// Finally any file/line number
	IMAGEHLP_LINE64 lineInfo = { sizeof(lineInfo) };
	DWORD dwDisplacement(0);
	if (SymGetLineFromAddr64(hProcess, reinterpret_cast<ULONG_PTR>(address), &dwDisplacement, &lineInfo))
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
Image file 및 address 필요
SymInitialize로 초기화 필수.
*/
void SimpleSymbolEngine::loadModule(HANDLE hFile, PVOID baseAddress, std::string const & fileName)
{
	::SymLoadModule64(hProcess, hFile, const_cast<char*>(fileName.c_str()), 0, reinterpret_cast<ULONG_PTR>(baseAddress), 0);
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 심볼테이블 UnLoad
Image base address 필요
SymInitialize로 초기화 필수.
*/
void SimpleSymbolEngine::unloadModule(PVOID baseAddress)
{
	::SymUnloadModule64(hProcess, reinterpret_cast<ULONG_PTR>(baseAddress));
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : void
기능 : 스택 호출을 통해 BasePointer(BP)(page Frame) , Instruction Register(IR)(Code Address) 주소 출력
CONTEXT => 레지스터 정보, 기본연산자 정보 제공
Context로 StackFrame을 초기화

*/
void SimpleSymbolEngine::stackTrace(HANDLE hThread, std::ostream & os)

{
	CONTEXT context = { 0 };
	PVOID pContext = &context;
	STACKFRAME64 stackFrame = { 0 };
#ifdef _M_IX86
	DWORD const machineType = IMAGE_FILE_MACHINE_I386;

	/*
	IMAGE_FILE_MACHINE_I386 = x86버전 의미
	CONTEXT_FULL = context 초기화 필요없다.
	*/
	context.ContextFlags = CONTEXT_FULL;
	GetThreadContext(hThread, &context);
	stackFrame.AddrPC.Offset = context.Eip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Ebp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Esp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
	/*
	stackFrame 초기화 과정
	AddrPC - Adrress Program Counter
	Eip - Instructin Register
	AddrModeFlat mode =
	Flat addressing. This is the only addressing mode supported by the library.
	Ebp = base 포인터
	Esp = stack 포인터
	*/

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
	os << "  Frame       Code address\n";
	while (::StackWalk64(machineType, hProcess, hThread,
		&stackFrame, pContext,
		0, ::SymFunctionTableAccess64, ::SymGetModuleBase64, 0))
		/*
		Frame 단위로 Stack trace를 얻어서 Address 출력
		lastBp = BasePointer 임시저장장소
		*/
	{
		if (stackFrame.AddrPC.Offset == 0)
		{
			os << "Null address\n";
			break;
		}
		PVOID frame = reinterpret_cast<PVOID>(stackFrame.AddrFrame.Offset);
		PVOID pc = reinterpret_cast<PVOID>(stackFrame.AddrPC.Offset);


		os << "  0x" << frame << "  " << addressToString(pc) << "\n";
		if (lastBp >= stackFrame.AddrFrame.Offset)
		{
			os << "Stack frame out of sequence...\n";
			break;
		}
		lastBp = stackFrame.AddrFrame.Offset;
	}

	os.flush();
}

/////////////////////////////////////////////////////////////////////////////////////
/*
반환 : string
기능 : 해당 주소(address)에 있는 string을 얻는다 .
wcstombs함수
WBCS 인 경우 MBCS로 변환
1번쨰 매개변수 NULL이면 바이트의 숫자를 반환
실패시 - 1 반환
*/
std::string SimpleSymbolEngine::getString(PVOID address, BOOL unicode, DWORD maxStringLength)
{
	if (unicode)
	{
		std::vector<wchar_t> chVector(maxStringLength + 1);
		ReadPartialProcessMemory(hProcess, address, &chVector[0], sizeof(wchar_t), maxStringLength * sizeof(wchar_t));

		size_t const wcLen = wcstombs(0, &chVector[0], 0);
		if (wcLen == (size_t)-1)
		{
			return "invalid string";
		}
		else
		{
			std::vector<char> mbStr(wcLen + 1);
			wcstombs(&mbStr[0], &chVector[0], wcLen);
			return &mbStr[0];
		}
	}
	else
	{
		std::vector<char> chVector(maxStringLength + 1);
		ReadPartialProcessMemory(hProcess, address, &chVector[0], 1, maxStringLength);
		return &chVector[0];
	}
}
