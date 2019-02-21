#include <iostream>
using namespace std;
void foo();
void DivideByZero(int ,int );
void main()
{
	foo();
}

void foo()
{
	DivideByZero(1, 0);
}
void DivideByZero(int n1, int n2)
{
	cout << n1 / n2 << endl;
}