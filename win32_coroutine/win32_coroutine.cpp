// win32_coroutine.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "windows.h"
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

using namespace std;

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

/**
 * 手动进行协程调度
 */
VOID
CoSyncExecute(
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	SwitchToFiber(Instance->ScheduleRoutine);
}

/**
 * 调度协程
 */
VOID
WINAPI CoScheduleRoutine(
	LPVOID lpFiberParameter
) {

	DWORD ByteTransfered;
	ULONG_PTR IoContext;
	LPOVERLAPPED Overlapped;
	DWORD Timeout = 0;

	//从TLS中获取协程实例
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	//如果有完成的IO端口事件,优先继续执行
DEAL_COMPLETED_IO:
	while (GetQueuedCompletionStatus(Instance->Iocp, &ByteTransfered, &IoContext, &Overlapped, Timeout)) {
		
		PASYNC_CONTEXT Context = (PASYNC_CONTEXT)CONTAINING_RECORD(Overlapped, ASYNC_CONTEXT, Overlapped);
		Context->BytesTransfered = ByteTransfered;

		SwitchToFiber(Context->Fiber);
	}

	//继续执行因为其他原因打断的协程或者新的协程
	PVOID Victim = (PVOID)Instance->FiberList->front();
	if (Victim) {

		Instance->FiberList->pop_front();
		SwitchToFiber(Victim);
		//如果有协程可执行，那么可能后面还有新的协程等待执行
		Timeout = 0;
	}
	else {

		//如果没有，那么就让完成端口等久一点
		Timeout = 500;
	}

	goto DEAL_COMPLETED_IO;
}

/**
 * 创建一个协程
 */
BOOLEAN
WINAPI
CoInsertRoutine(
	PCOROUTINE_INSTANCE Instance,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter
) {

	//创建一个Coroutine
	PVOID NewFiber = CreateFiber(0, StartRoutine, Parameter);
	if (NewFiber == NULL)
		return FALSE;

	//保存进Coroutine队列
	Instance->FiberList->push_back(NewFiber);

	return TRUE;
}

/**
 * 纤程入口点
 */
DWORD
WINAPI 
CoThreadEntryPoint(
	LPVOID lpThreadParameter
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)lpThreadParameter;
	TlsSetValue(0, Instance);

	ConvertThreadToFiber(NULL);
	
	Instance->ScheduleRoutine = CreateFiber(0x1000, CoScheduleRoutine, NULL);
	if (Instance->ScheduleRoutine == NULL)
		return 0;

	//test
	CoInsertRoutine(Instance, CoRoutine1, NULL);
	CoInsertRoutine(Instance, CoRoutine2, NULL);

	SwitchToFiber(Instance->ScheduleRoutine);

	ConvertFiberToThread();
	return 0;
}

/**
 * 创建一个协程
 * @param[in]	NewThread		是否创建一个新的线程
 */
HANDLE
CoCreateCoroutine(
	LPFIBER_START_ROUTINE InitRoutine,
	BOOLEAN NewThread
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)malloc(sizeof(COROUTINE_INSTANCE));
	DWORD ThreadId;

	//创建线程相关的IO完成端口
	Instance->Iocp= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Instance->Iocp == NULL)
		goto ERROR_EXIT;

	//创建协程调度队列
	Instance->FiberList = new list<void*>;

	//创建协程宿主线程
	Instance->ThreadHandle = CreateThread(NULL, 0, CoThreadEntryPoint, Instance, 0, &ThreadId);
	if (Instance->ThreadHandle == NULL) {
		goto ERROR_EXIT;
	}

	return Instance;

ERROR_EXIT:
	if (Instance->Iocp)
		CloseHandle(Instance->Iocp);

	free(Instance);
	return NULL;
}



