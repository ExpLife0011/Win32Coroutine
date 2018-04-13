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
 * �ֶ�����Э�̵���
 */
VOID
CoSyncExecute(
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

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

	//��TLS�л�ȡЭ��ʵ��
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	//�������ɵ�IO�˿��¼�,���ȼ���ִ��
DEAL_COMPLETED_IO:
	while (GetQueuedCompletionStatus(Instance->Iocp, &ByteTransfered, &IoContext, &Overlapped, Timeout)) {
		
		PASYNC_CONTEXT Context = (PASYNC_CONTEXT)CONTAINING_RECORD(Overlapped, ASYNC_CONTEXT, Overlapped);
		Context->BytesTransfered = ByteTransfered;

		SwitchToFiber(Context->Fiber);
	}

	//����ִ����Ϊ����ԭ���ϵ�Э�̻����µ�Э��
	if (!Instance->FiberList->empty()) {

		PVOID Victim = (PVOID)Instance->FiberList->front();
		Instance->FiberList->pop_front();
		SwitchToFiber(Victim);
		
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
WINAPI
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

	//����Э�������߳�
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



