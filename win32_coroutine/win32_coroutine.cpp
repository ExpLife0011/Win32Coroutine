// win32_coroutine.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "windows.h"
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

using namespace std;

/**
 * �̺߳���ִ������
 */
VOID
WINAPI CoCompatRoutineHost(
	LPVOID lpFiberParameter
) {

	//��ȡ����
	PCOROUTINE_COMPAT_CALL CompatCall = (PCOROUTINE_COMPAT_CALL)lpFiberParameter;
	CompatCall->CompatRoutine(CompatCall->Parameter);

	free(CompatCall);

	CoSyncExecute(TRUE);
}

/**
 * �ֶ�����Э�̵���
 */
VOID
CoSyncExecute(
	BOOLEAN Terminate
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	Instance->LastFiberFinished = Terminate;

	SwitchToFiber(Instance->ScheduleRoutine);
}

/**
 * ����Э��
 */
VOID
WINAPI CoScheduleRoutine(
	LPVOID lpFiberParameter
) {

	DWORD ByteTransfered;
	ULONG_PTR IoContext;
	LPOVERLAPPED Overlapped;
	DWORD Timeout = 0;
	PVOID Victim;

	//��TLS�л�ȡЭ��ʵ��
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	//�������ɵ�IO�˿��¼�,���ȼ���ִ��
DEAL_COMPLETED_IO:
	while (GetQueuedCompletionStatus(Instance->Iocp, &ByteTransfered, &IoContext, &Overlapped, Timeout)) {
		
		PCOROUTINE_OVERLAPPED_WARPPER Context = (PCOROUTINE_OVERLAPPED_WARPPER)
			CONTAINING_RECORD(Overlapped, COROUTINE_OVERLAPPED_WARPPER, Overlapped);
		Context->BytesTransfered = ByteTransfered;
		Context->ErrorCode = GetLastError();

		//����ṹ������Э��ִ���б��ͷ���
		Victim = Context->Fiber;
		SwitchToFiber(Victim);

		if (Instance->LastFiberFinished == TRUE)
			DeleteFiber(Victim);
	}

	//����ִ����Ϊ����ԭ���ϵ�Э�̻����µ�Э��
	if (!Instance->FiberList->empty()) {

		Victim = (PVOID)Instance->FiberList->front();
		Instance->FiberList->pop_front();
		SwitchToFiber(Victim);
		
		if (Instance->LastFiberFinished == TRUE)
			DeleteFiber(Victim);

		//�����Э�̿�ִ�У���ô���ܺ��滹���µ�Э�̵ȴ�ִ��
		Timeout = 0;
	}
	else {

		//���û�У���ô������ɶ˿ڵȾ�һ��
		Timeout = 500;
	}

	goto DEAL_COMPLETED_IO;
}

/**
 * ����һ��Э��
 */
BOOLEAN
CoInsertRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
) {

	PCOROUTINE_INSTANCE CoInstance;
	if (Instance)
		CoInstance = Instance;
	else
		CoInstance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	if (CoInstance == NULL)
		return FALSE;

	//����һ��Coroutine
	PVOID NewFiber = CreateFiber(StackSize, StartRoutine, Parameter);
	if (NewFiber == NULL)
		return FALSE;

	//�����Coroutine����
	CoInstance->FiberList->push_back(NewFiber);
	
	return TRUE;
}

/**
 * ����һ�������߳�ABI��Э��
 */
BOOLEAN
CoInsertCompatRoutine(
	SIZE_T StackSize,
	LPTHREAD_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
) {

	PCOROUTINE_COMPAT_CALL CompatCall = (PCOROUTINE_COMPAT_CALL)malloc(sizeof(COROUTINE_COMPAT_CALL));
	CompatCall->CompatRoutine = StartRoutine;
	CompatCall->Parameter = Parameter;

	return CoInsertRoutine(StackSize, CoCompatRoutineHost, CompatCall, Instance);
}


/**
 * �˳���ڵ�
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

	Instance->FiberList->push_back(Instance->InitialRoutine);

	SwitchToFiber(Instance->ScheduleRoutine);

	ConvertFiberToThread();
	return 0;
}

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
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)malloc(sizeof(COROUTINE_INSTANCE));
	DWORD ThreadId;

	//�����߳���ص�IO��ɶ˿�
	Instance->Iocp= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Instance->Iocp == NULL)
		goto ERROR_EXIT;

	//����Э�̵��ȶ���
	Instance->FiberList = new list<void*>;
	Instance->InitialRoutine = CreateFiber(StackSize, InitRoutine, Parameter);

	if (NewThread) {

		//����Э�������߳�
		Instance->ThreadHandle = CreateThread(NULL, 0, CoThreadEntryPoint, Instance, 0, &ThreadId);
		if (Instance->ThreadHandle == NULL) {
			goto ERROR_EXIT;
		}
	}
	else {

		//������������̣߳�����ֻ�ǳ�ʼ������Ҫ�ֶ���ʼ����
		TlsSetValue(0, Instance);

		Instance->ScheduleRoutine = CreateFiber(0x1000, CoScheduleRoutine, NULL);
		if (Instance->ScheduleRoutine == NULL)
			goto ERROR_EXIT;

		Instance->FiberList->push_back(Instance->InitialRoutine);
	}

	return (HANDLE)Instance;

ERROR_EXIT:
	if (Instance->Iocp)
		CloseHandle(Instance->Iocp);

	free(Instance);
	return NULL;
}



