#include <iostream>
#include <string>
using namespace std;
void foo(string str);
void main()
{
	string str;
	cout << "���ڿ� �Է� : ";
	cin >> str;
	foo(str);
}
void foo(string str)
{
	cout << str << endl;
}
