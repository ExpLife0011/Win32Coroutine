
#include "stdafx.h"
#include "win32co_coroutine.h"

DWORD begin_time, end_time;
DWORD end_task;
HANDLE Event;

using namespace Win32Coroutine;

VOID
WINAPI CoRoutine1(
	LPVOID lpFiberParameter
) {

	CHAR buf[4096];
	DWORD Size;
	LONG Offset = 0;
	SIZE_T CSize;
	LARGE_INTEGER FileSize;

	PVOID CompressData;

	HANDLE g = CreateFile(L"G:\\1.DMP", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

	GetFileSizeEx(g, &FileSize);

	for (int i = 0;i < 2000;i++) {
		SetFilePointer(g, (i & 1) ? (4096 * i) : ((1 << 30) - 4096 * i), &Offset, FILE_BEGIN);
		ReadFile(g, buf, 4096, &Size, NULL);

		for (int q = 0;q < 100;q++) {
			for (int j = 0;j < 4000;j++) {
				buf[j] += buf[j + 1];
			}
		}

	}

	end_task++;

	if (end_time < GetTickCount()) {
		end_time = GetTickCount();
	}

	if (IsThreadAFiber() && (end_task == 2))
		SetEvent(Event);

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
	LARGE_INTEGER FileSize;

	PVOID CompressData;

	HANDLE g = CreateFile(L"G:\\1.DMP", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

	GetFileSizeEx(g, &FileSize);

	for (int i = 0;i < 2000;i++) {
		SetFilePointer(g, (i & 1) ? (4096 * i) : ((1 << 30) - 4096 * i), &Offset, FILE_BEGIN);
		ReadFile(g, buf, 4096, &Size, NULL);

		for (int q = 0;q < 100;q++) {
			for (int j = 0;j < 4000;j++) {
				buf[j] += buf[j + 1];
			}
		}

	}

	end_task++;

	if (end_time < GetTickCount()) {
		end_time = GetTickCount();
	}

	if (IsThreadAFiber() && (end_task == 2))
		SetEvent(Event);

	CloseHandle(g);
}

VOID
WINAPI
InitRoutine(
	LPVOID lParameter
) {

	begin_time = GetTickCount();

	Coroutine::CoInsertStandardRoutine(0x1000, CoRoutine1, NULL, NULL);
	Coroutine::CoInsertStandardRoutine(0x1000, CoRoutine2, NULL, NULL);

}

VOID 
test_fileio() {

	printf("launch fileio test:\n");

	Event = CreateEvent(NULL, FALSE, FALSE, NULL);

	//运行非协程的测试
	begin_time = GetTickCount();

	CoRoutine1(NULL);
	CoRoutine2(NULL);
	
	printf("\trun time without coroutine:%d\n", end_time - begin_time);
	
	//运行协程的测试
	end_task = 0;
	Coroutine::CoCreateCoroutine(NULL, InitRoutine, NULL, TRUE);
	WaitForSingleObject(Event, INFINITE);
	
	printf("\trun time with coroutine:%d\n", end_time - begin_time);

	printf("\n");
}