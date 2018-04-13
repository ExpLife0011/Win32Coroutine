#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

typedef struct _COROUTINE_INSTANCE {

	HANDLE Iocp;
	HANDLE ThreadHandle;

	PVOID ScheduleRoutine;

	std::list<void*>* FiberList;
}COROUTINE_INSTANCE, *PCOROUTINE_INSTANCE;

typedef struct _ASYNC_CONTEXT {

	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	PVOID Fiber;
}ASYNC_CONTEXT, *PASYNC_CONTEXT;

/**
 * 手动进行协程调度
 */
VOID
CoSyncExecute(
);

/**
 * 创建一个协程
 */
BOOLEAN
WINAPI
CoInsertRoutine(
	PCOROUTINE_INSTANCE Instance,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter
);

/**
 * 创建一个协程
 * @param[in]	NewThread		是否创建一个新的线程
 */
HANDLE
CoCreateCoroutine(
	BOOLEAN NewThread
);