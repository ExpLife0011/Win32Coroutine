#pragma once

#include <Windows.h>
#include <stdlib.h>

#define COEXPORT __declspec(dllexport)

#include "win32co_list.h"
#include "win32co_hook.h"
#include "win32co_sysroutine.h"
#include "win32co_error.h"
#include "win32co_queue.h"

#define PERF_TEST

#define ASIO_FILE		1
#define ASIO_NET		2

#define SIGN_OVERLAPPED	0xff981234

#define STORE_FIBER_INSTANCE(_inst_)			(FlsSetValue(Co_FiberInstanceIndex,(PVOID)(_inst_)))
#define RETRIEVE_FIBER_INSTANCE()				((HANDLE)FlsGetValue(Co_FiberInstanceIndex))
#define GET_FIBER_FROM_INSTANCE(_handle_)		(((PCOROUTINE_FIBER_INSTANCE)(_handle_))->FiberRoutine)
#define GET_PARA_FROM_INSTANCE(_handle_)		(((PCOROUTINE_FIBER_INSTANCE)(_handle_))->Parameter)

namespace Win32Coroutine {

	typedef struct _COROUTINE_INSTANCE COROUTINE_INSTANCE, *PCOROUTINE_INSTANCE;

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

	//��ʱִ�ж���
	typedef struct _COROUTINE_EXECUTE_DELAY {
		LIST_ENTRY Entry;
		DWORD64 TimeAtLeast;
		LPVOID Fiber;
	}COROUTINE_EXECUTE_DELAY, *PCOROUTINE_EXECUTE_DELAY;

	//��Ϣ���ж���
	//����Queue��
	typedef struct _COROUTINE_MESSAGE_QUEUE {
		SLIST_HEADER QueueHeader;
		SLIST_HEADER PendingQueueHeader;
		SLIST_HEADER WorkerQueueHeader;
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
		PVOID FiberRoutine;
		COROUTINE_MESSAGE_QUEUE InternalMessageQueue;

		LPFIBER_START_ROUTINE UserStanderRoutine;
		LPTHREAD_START_ROUTINE UserCompatRoutine;
		PVOID UserParameter;
	}COROUTINE_FIBER_INSTANCE, *PCOROUTINE_FIBER_INSTANCE;

	//�����е�Fiber
	typedef struct _COROUTINE_PENDING_FIBER {
		SLIST_ENTRY Entry;
		LPVOID PendingFiber;
	}COROUTINE_PENDING_FIBER, *PCOROUTINE_PENDING_FIBER;

	//һ��Э��ʵ��
	struct _COROUTINE_INSTANCE {
		HANDLE Iocp;
		HANDLE ThreadHandle;

		LPVOID HostThread;

		HANDLE ScheduleRoutine;
		
		PVOID InitialRoutine;
		PVOID InitialParameter;

		LIST_ENTRY FiberInstanceList;
		LIST_ENTRY DelayExecutionList;

		BOOLEAN LastFiberFinished;
		HANDLE LastFiber;
	};

	namespace Coroutine {

		/**
		 * �ֶ�����Э�̵���
		 */
		COEXPORT
		VOID
			WINAPI
			CoYield(
				BOOLEAN Terminate
			);

		/**
		 * ��ִ���˳�ǰ���ʼ��
		 */
		COEXPORT
		VOID
			WINAPI
			CoPreInitialize(
				LPVOID lpParameter
			);

		/**
		 * ����һ��Э��
		 * ��ʵ�ϵ��Է��֣�StackSize����ָ�����٣�ֻҪջ�ռ䲻����ϵͳ��
		 * �Զ������µ�ջ�ռ䣬�����ᴥ���쳣���������ֵ����ȡֵ��PageSize
		 */
		COEXPORT
		PCOROUTINE_FIBER_INSTANCE
			WINAPI
			CoCreateFiberInstance(
				SIZE_T StackSize,
				LPFIBER_START_ROUTINE StartRoutine,
				LPVOID Parameter
			);

		/**
		 * ����һ�������߳�ABI��Э��
		 */
		COEXPORT
		HANDLE
			WINAPI
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
		COEXPORT
		HANDLE
			WINAPI
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
		COEXPORT
		BOOLEAN
			WINAPI
			CoDelayExecutionAtLeast(
				PVOID Fiber,
				DWORD MillionSecond
			);

		/**
		 * �ֶ���������
		 */
		COEXPORT
		BOOLEAN
			WINAPI
			CoStartCoroutineManually(
			);

		/**
		 * ����һ��Э��
		 * @param[in]	NewThread		�Ƿ񴴽�һ���µ��߳�
		 */
		COEXPORT
		HANDLE
			WINAPI
			CoCreateCoroutine(
				SIZE_T StackSize,
				LPFIBER_START_ROUTINE InitRoutine,
				LPVOID Parameter,
				BOOLEAN NewThread
			);

		/**
		 * ��ʼ��Э�̿�
		 */
		COEXPORT
		VOID
			WINAPI
			CoInitialize(
			);

	}

}