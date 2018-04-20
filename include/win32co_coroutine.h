#pragma once

#include <Windows.h>
#include <stdlib.h>

#include "win32co_list.h"
#include "win32co_hook.h"
#include "win32co_sysroutine.h"
#include "win32co_error.h"
#include "win32co_queue.h"

#define PERF_TEST

#define ASIO_FILE		1
#define ASIO_NET		2

#define SIGN_OVERLAPPED	0xff981234

#define COEXPORT __declspec(dllexport)

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
	LPTHREAD_START_ROUTINE UserRoutine;
	LPVOID UserParameter;
}COROUTINE_COMPAT_CALL, *PCOROUTINE_COMPAT_CALL;

//��׼��ʽ���˳̵���
typedef struct _COROUTINE_STANDARD_CALL {
	LPFIBER_START_ROUTINE UserRoutine;
	LPVOID UserParameter;
}COROUTINE_STANDARD_CALL, *PCOROUTINE_STANDARD_CALL;

//��ʱִ�ж���
typedef struct _COROUTINE_EXECUTE_DELAY {
	LIST_ENTRY Entry;
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
typedef struct _COROUTINE_FIBER_INSTANCE {
	LIST_ENTRY Entry;
	LPVOID FiberRoutine;
	LPVOID Parameter;
	COROUTINE_MESSAGE_QUEUE InternalMessageQueue;
}COROUTINE_FIBER_INSTANCE, *PCOROUTINE_FIBER_INSTANCE;

//�����е�Fiber
typedef struct _COROUTINE_PENDING_FIBER {
	SLIST_ENTRY Entry;
	LPVOID PendingFiber;
}COROUTINE_PENDING_FIBER,*PCOROUTINE_PENDING_FIBER;

//һ��Э��ʵ��
typedef struct _COROUTINE_INSTANCE {
	HANDLE Iocp;
	HANDLE ThreadHandle;

	LPVOID HostThread;

	HANDLE ScheduleRoutine;
	HANDLE InitialRoutine;

	LIST_ENTRY FiberInstanceList;
	LIST_ENTRY DelayExecutionList;

	BOOLEAN LastFiberFinished;
	HANDLE LastFiber;
}COROUTINE_INSTANCE, *PCOROUTINE_INSTANCE;

#define STORE_FIBER_INSTANCE(_inst_)			(FlsSetValue(Co_FiberInstanceIndex,(PVOID)(_inst_)))
#define RETRIEVE_FIBER_INSTANCE()				((HANDLE)FlsGetValue(Co_FiberInstanceIndex))
#define GET_FIBER_FROM_INSTANCE(_handle_)		(((PCOROUTINE_FIBER_INSTANCE)(_handle_))->FiberRoutine)
#define GET_PARA_FROM_INSTANCE(_handle_)		(((PCOROUTINE_FIBER_INSTANCE)(_handle_))->Parameter)

/**
 * �ֶ�����Э�̵���
 */
VOID
CoYield(
	BOOLEAN Terminate
);

/**
 * ��ִ���˳�ǰ���ʼ��
 */
VOID
CoPreInitialize(
	LPVOID lpParameter
);

/**
 * ����һ��Э��
 * ��ʵ�ϵ��Է��֣�StackSize����ָ�����٣�ֻҪջ�ռ䲻����ϵͳ��
 * �Զ������µ�ջ�ռ䣬�����ᴥ���쳣���������ֵ����ȡֵ��PageSize
 */
PCOROUTINE_FIBER_INSTANCE
CoCreateFiberInstance(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter
);

/**
 * ����һ�������߳�ABI��Э��
 */
HANDLE
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
HANDLE
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
BOOLEAN
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

/**
 * ��ʼ��Э�̿�
 */
VOID
CoInitialize(
);