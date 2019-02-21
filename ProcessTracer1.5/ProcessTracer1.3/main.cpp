#include "ProcessTracer.h"
#include <iostream>
enum { PARAMETER = 1, CMD, MISTAKE, NORMAL, EXIT };
using namespace std;
int choice();
/*
main
명령어 전달 방식 선택
ProcessTracer 클래스 생성
매개변수로 명령어 전달
예외처리
*/
int _tmain(int argc, TCHAR **argv)
{
	TCHAR* temp = argv[1];
	while (true)
	{
		if (!ProcessTracer::IsExistArgv(argc))
			return 1;
		switch (choice())
		{
		case PARAMETER: argv[1] = temp; break;
		case CMD: argv[1] = _T("cmd.exe"); break;
		case MISTAKE: argv[1] = _T("Error.exe"); break;
		case NORMAL: argv[1] = _T("Normal.exe"); break;
		case EXIT:
			cout << ("종료") << endl;
			exit(1); break;
		}
		try
		{
			ProcessTracer* handler = new ProcessTracer(argc, argv);
			/*
			ProcessTracer* handler = new ProcessTracer;
			handler->MyCreateProcess(argc, argv);
			*/
			handler->run();
		}
		catch (std::exception &ex)
		{
			std::cerr << ("Unexpected exception: ") << ex.what() << std::endl;
			return 1;
		}
	}
	return 0;
}
/*
반환 : int
기능 : 명령어 전달 방식 선택
*/
int choice()
{
	cout << ("1. command line parameter ") << endl;
	cout << ("2. 직접 입력 (CMD) ") << endl;
	cout << ("3. MISTAKE COMMAND SET ") << endl;
	cout << ("4. NORMAL COMMAND SET ") << endl;
	cout << ("5. EXIT") << endl;
	cout << ("명령어로 무엇을 사용하시겠습니까? :");
	int choice;
	cin >> choice;
	return choice;
}

