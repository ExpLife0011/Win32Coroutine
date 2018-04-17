#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

#define PERF_TEST

#define ASIO_FILE		1
#define ASIO_NET		2

//�ص�������չ�ṹ
typedef struct _COROUTINE_OVERLAPPED_WARPPER {
	DWORD Signature;
	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	DWORD ErrorCode;
	DWORD AsioType;
	PVOID Fiber;
	HANDLE Handle;
}COROUTINE_OVERLAPPED_WARPPER, *PCOROUTINE_OVERLAPPED_WARPPER;

//�����̸߳�ʽ���˳̵���
typedef struct _COROUTINE_COMPAT_CALL {
	LPTHREAD_START_ROUTINE CompatRoutine;
	LPVOID Parameter;
}COROUTINE_COMPAT_CALL, *PCOROUTINE_COMPAT_CALL;

//��׼��ʽ���˳̵���
typedef struct _COROUTINE_STANDARD_CALL {
	LPFIBER_START_ROUTINE FiberRoutine;
	LPVOID Parameter;
}COROUTINE_STANDARD_CALL, *PCOROUTINE_STANDARD_CALL;

//��ʱִ�ж���
typedef struct _COROUTINE_EXECUTE_DELAY {
	DWORD64 TimeAtLeast;
	LPVOID Fiber;
}COROUTINE_EXECUTE_DELAY, *PCOROUTINE_EXECUTE_DELAY;

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
CoYield(
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
 * ����һ����ͨ���˳�
 * Ϊ�˱�֤�˳̶����ܼ�ʱ�Ļ��գ�������������ӿ�
 */
BOOLEAN
CoInsertStandardRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
);

/**
 * ���һ����ʱִ���¼�
 * @param	Fiber			Э��
 * @param	MillionSecond	��ʱ������
 * @note	����Э�̲��ǻ���ʱ��Ƭ���ȣ��������ֻ����ʱ��Сʱ�䣬�������ܻ�����ʱ��Ҫ��
 */
VOID
CoDelayExecutionAtLeast(
	PVOID Fiber,
	DWORD MillionSecond
);

/**
 * �ֶ���������
 */
BOOLEAN
CoStartCoroutineManually(
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