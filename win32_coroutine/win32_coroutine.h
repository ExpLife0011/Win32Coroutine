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
	PCOROUTINE_INSTANCE Instance,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter
);

/**
 * ����һ��Э��
 * @param[in]	NewThread		�Ƿ񴴽�һ���µ��߳�
 */
HANDLE
CoCreateCoroutine(
	BOOLEAN NewThread
);