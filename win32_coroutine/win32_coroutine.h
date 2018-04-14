#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

#define PERF_TEST

typedef struct _COROUTINE_INSTANCE {
	HANDLE Iocp;
	HANDLE ThreadHandle;

	PVOID ScheduleRoutine;
	PVOID InitialRoutine;

	std::list<void*>* FiberList;

	BOOLEAN LastFiberFinished;
}COROUTINE_INSTANCE, *PCOROUTINE_INSTANCE;

typedef struct _COROUTINE_OVERLAPPED_WARPPER {
	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	PVOID Fiber;
}COROUTINE_OVERLAPPED_WARPPER, *PCOROUTINE_OVERLAPPED_WARPPER;

typedef struct _COROUTINE_COMPAT_CALL {
	LPTHREAD_START_ROUTINE CompatRoutine;
	LPVOID Parameter;
}COROUTINE_COMPAT_CALL, *PCOROUTINE_COMPAT_CALL;

/**
 * �ֶ�����Э�̵���
 */
VOID
CoSyncExecute(
	BOOLEAN Terminate
);

/**
 * ����һ��Э��
 */
BOOLEAN
CoInsertRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
);

/**
 * ����һ�������߳�ABI��Э��
 */
BOOLEAN
CoInsertCompatRoutine(
	SIZE_T StackSize,
	LPTHREAD_START_ROUTINE StartRoutine,
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