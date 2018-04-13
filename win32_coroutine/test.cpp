#pragma once

#include "stdafx.h"
#include "win32_coroutine.h"


VOID
WINAPI CoRoutine1(
	LPVOID lpFiberParameter
) {

	CHAR buf[512];
	DWORD Size;

	HANDLE g = CreateFile(L"C:\\1.txt", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	ReadFile(g, buf, 512, &Size, NULL);

	CloseHandle(g);
}

VOID
WINAPI CoRoutine2(
	LPVOID lpFiberParameter
) {

	CHAR buf[512];
	DWORD Size;

	HANDLE g = CreateFile(L"C:\\1.txt", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	ReadFile(g, buf, 512, &Size, NULL);

	CloseHandle(g);
}

VOID
WINAPI
InitRoutine(
	LPVOID lParameter
) {

	//test
	CoInsertRoutine(0x1000, CoRoutine1, NULL, NULL);
	CoInsertRoutine(0x1000, CoRoutine2, NULL, NULL);

	CoSyncExecute();
}

int main()
{

	CoSetupWin32ApiHook();

	HANDLE CoHandle = CoCreateCoroutine(0x1000, InitRoutine, NULL, TRUE);

	getchar();
	return 0;
}