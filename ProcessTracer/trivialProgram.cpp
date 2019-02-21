/**
 * This is a minimal program ...
 * It contains an entry point with a return, and no dependent DLLs
 */

static char const szRCSID[] = "$Id: trivialProgram.cpp 87 2011-11-19 10:04:36Z Roger $";

#pragma comment(linker, "/nodefaultlib")

int main() {return 0;}

int mainCRTStartup() {return 0;}