#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

#define PERF_TEST

//�ص�������չ�ṹ
typedef struct _COROUTINE_OVERLAPPED_WARPPER {
	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	DWORD ErrorCode;
	PVOID Fiber;
	PVOID AcceptBuffer;
}COROUTINE_OVERLAPPED_WARPPER, *PCOROUTINE_OVERLAPPED_WARPPER;

//�����̸߳�ʽ���˳̵���
typedef struct _COROUTINE_COMPAT_CALL {
	LPTHREAD_START_ROUTINE CompatRoutine;
	LPVOID Parameter;
}COROUTINE_COMPAT_CALL, *PCOROUTINE_COMPAT_CALL;

//��ʱִ�ж���
typedef struct _COROUTINE_EXECUTE_DELAY {
	DWORD64 TimeAtLeast;
	LPVOID Fiber;
}COROUTINE_EXECUTE_DELAY, *PCOROUTINE_EXECUTE_DELAY;

//��Ϣ���ж���
typedef struct _COROUTINE_MESSAGE_QUEUE {
	SLIST_HEADER QueueHeader;
	PVOID Fiber;
}COROUTINE_MESSAGE_QUEUE, *PCOROUTINE_MESSAGE_QUEUE;

//��Ϣ���нڵ�
typedef struct _COROUTINE_MESSAGE_NODE {
	SLIST_ENTRY QueueNode;
	PVOID UserBuffer;
	SIZE_T BufferSize;
}COROUTINE_MESSAGE_NODE, *PCOROUTINE_MESSAGE_NODE;

//�˳�������
typedef struct _COROUTINE_FIBER_CONTEXT {
	LPVOID UserParameter;
	COROUTINE_MESSAGE_QUEUE InternalMessageQueue;
}COROUTINE_FIBER_CONTEXT;

//һ��Э��ʵ��
typedef struct _COROUTINE_INSTANCE {
	HANDLE Iocp;
	HANDLE ThreadHandle;

	PVOID ScheduleRoutine;
	PVOID InitialRoutine;

	std::list<void*>* FiberList;
	std::list<PCOROUTINE_EXECUTE_DELAY>* DelayExecutionList;
	
	BOOLEAN LastFiberFinished;
}COROUTINE_INSTANCE, *PCOROUTINE_INSTANCE;

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