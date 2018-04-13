#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

typedef struct _COROUTINE_INSTANCE {

	HANDLE Iocp;
	HANDLE ThreadHandle;

	PVOID ScheduleRoutine;
	PVOID InitialRoutine;

	std::list<void*>* FiberList;
}COROUTINE_INSTANCE, *PCOROUTINE_INSTANCE;

typedef struct _ASYNC_CONTEXT {

	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	PVOID Fiber;
}ASYNC_CONTEXT, *PASYNC_CONTEXT;

/**
 * �ֶ�����Э�̵���
 */
VOID
CoSyncExecute(
);

/**
 * ����һ��Э��
 */
BOOLEAN
WINAPI
CoInsertRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
);

/**
 * ����һ��Э��
 * @param[in]	NewThread		�Ƿ񴴽�һ���µ��߳�
 */
HANDLE
CoCreateCoroutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE InitRoutine,
	LPVOID Parameter,
	BOOLEAN NewThread
);