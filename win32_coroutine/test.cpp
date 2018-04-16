#pragma once

#include "stdafx.h"
#include "win32_coroutine.h"
#include <compressapi.h>


//#pragma comment(lib,"Cabinet.lib")
#pragma comment(lib,"ws2_32.lib")

//#define TEST
#define THREAD

DWORD s;
DWORD end_time = 0;

HANDLE Event;

int COUNT = 5000;

VOID
WINAPI CoRoutine1(
	LPVOID lpFiberParameter
) {

	CHAR buf[4096];
	DWORD Size;
	LONG Offset = 0;
	SIZE_T CSize;

	PVOID CompressData;
	
	HANDLE g = CreateFile(L"G:\\1.DMP", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	for (int i = 0;i < COUNT;i++) {
		SetFilePointer(g, 4096 * i, &Offset, FILE_BEGIN);
		ReadFile(g, buf, 4096, &Size, NULL);

		//SetFilePointer(g, 1 * 1024 * 1024 * 1024 - 4096 * i, &Offset, FILE_BEGIN);
		//ReadFile(g, buf, 4096, &Size, NULL);

#ifdef TEST
		COMPRESSOR_HANDLE Com = NULL;
		CompressData = malloc(4096);
		CreateCompressor(COMPRESS_ALGORITHM_LZMS, NULL, &Com);
		if (Com != NULL) {
			Compress(Com, buf, 4096, CompressData, 4096, &CSize);
			CloseCompressor(Com);
		}
		free(CompressData);
#endif

	}

	if (end_time < GetTickCount()) {
		end_time = GetTickCount();
#ifdef THREAD
		SetEvent(Event);
#endif
	}

	CloseHandle(g);
}

VOID
WINAPI CoRoutine2(
	LPVOID lpFiberParameter
) {

	CHAR buf[4096];
	DWORD Size;
	LONG Offset = 0;
	SIZE_T CSize;

	PVOID CompressData;

	HANDLE g = CreateFile(L"G:\\1.DMP", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	for (int i = 0;i < COUNT;i++) {
		SetFilePointer(g, 4096 * i, &Offset, FILE_BEGIN);
		ReadFile(g, buf, 4096, &Size, NULL);

		//SetFilePointer(g, 1 * 1024 * 1024 * 1024 - 4096 * i, &Offset, FILE_BEGIN);
		//ReadFile(g, buf, 4096, &Size, NULL);

#ifdef TEST
		COMPRESSOR_HANDLE Com = NULL;
		CompressData = malloc(4096);
		CreateCompressor(COMPRESS_ALGORITHM_LZMS, NULL, &Com);
		if (Com != NULL) {
			Compress(Com, buf, 4096, CompressData, 4096, &CSize);
			CloseCompressor(Com);
		}
		free(CompressData);
#endif

	}
	
	if (end_time < GetTickCount()) {
		end_time = GetTickCount();
#ifdef THREAD
		SetEvent(Event);
#endif
	}

	CloseHandle(g);
}

VOID
WINAPI
InitRoutine(
	LPVOID lParameter
) {

	s = GetTickCount();
	//test
	for (int i = 0;i < 10000;i++) {
		CoInsertRoutine(0x1000, CoRoutine1, NULL, NULL);
	}
	for (int i = 0;i < 10000;i++) {
		CoInsertRoutine(0x1000, CoRoutine2, NULL, NULL);
	}

	CoSyncExecute(TRUE);
}

int main()
{

	Event = CreateEvent(NULL, FALSE, FALSE, NULL);

#ifdef THREAD
	CoSetupWin32ApiHook(NULL);
	HANDLE CoHandle = CoCreateCoroutine(0x1000, InitRoutine, NULL, TRUE);

	WaitForSingleObject(Event, INFINITE);
	printf("%d\n", end_time - s);

#else

	for (int i = 1;i < 6;i++) {
		COUNT = i * 1000;
		s = GetTickCount();
		CoRoutine1(NULL);
		CoRoutine2(NULL);
		printf("%d\n", end_time - s);
	}

#endif

	

	getchar();
	return 0;
}