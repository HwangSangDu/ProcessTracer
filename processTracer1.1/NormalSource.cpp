#include <iostream>
#include <string>
using namespace std;
void foo(string str);
void main()
{
	string str;
	cout << "문자열 입력 : ";
	cin >> str;
	foo(str);
}
void foo(string str)
{
	cout << str << endl;
}
