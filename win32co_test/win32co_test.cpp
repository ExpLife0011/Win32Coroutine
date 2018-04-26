// win32co_test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <Windows.h>
#include "win32co_coroutine.h"

#pragma comment(lib,"..\\x64\\debug\\win32co.lib")

using namespace Win32Coroutine;

int main()
{
	
	Coroutine::CoInitialize();
	
	Hook::CoSetupFileIoHook(NULL);

	test_fileio();

	getchar();

    return 0;
}

