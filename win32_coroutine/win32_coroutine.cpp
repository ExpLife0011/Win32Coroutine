// win32_coroutine.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "windows.h"
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

using namespace std;

VOID
WINAPI CoRoutine1(
	LPVOID lpFiberParameter
) {

	CHAR buf[512];
	DWORD Size;

	HANDLE g = CreateFile(L"C:\\1.txt", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	ReadFile(g, buf, 512, &Size, NULL);

	CloseHandle(g);
}

VOID
WINAPI CoRoutine2(
	LPVOID lpFiberParameter
) {

	CHAR buf[512];
	DWORD Size;

	HANDLE g = CreateFile(L"C:\\1.txt", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	ReadFile(g, buf, 512, &Size, NULL);

	CloseHandle(g);
}

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
	PVOID Victim = (PVOID)Instance->FiberList->front();
	if (Victim) {

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
	PCOROUTINE_INSTANCE Instance,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter
) {

	//����һ��Coroutine
	PVOID NewFiber = CreateFiber(0, StartRoutine, Parameter);
	if (NewFiber == NULL)
		return FALSE;

	//�����Coroutine����
	Instance->FiberList->push_back(NewFiber);

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

	//test
	CoInsertRoutine(Instance, CoRoutine1, NULL);
	CoInsertRoutine(Instance, CoRoutine2, NULL);

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
	LPFIBER_START_ROUTINE InitRoutine,
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



