// win32_coroutine.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "windows.h"
#include <ntstatus.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

typedef DWORD NTSTATUS;

using namespace std;

SIZE_T Co_SystemPageSize = 4096;
SIZE_T Co_ProcessorCount;

typedef
ULONG 
(WINAPI* LpfnRtlNtStatusToDosError)(
	_In_ NTSTATUS Status
);

LpfnRtlNtStatusToDosError RtlNtStatusToDosError = NULL;

/**
 * ��NT������ת��ΪWinsock������
 * ��libuv��������
 * @param	status	Nt������
 */
DWORD
CoNtatusToWinsockError(
	NTSTATUS Status
) {
	switch (Status) {
	case STATUS_SUCCESS:
		return ERROR_SUCCESS;

	case STATUS_PENDING:
		return ERROR_IO_PENDING;

	case STATUS_INVALID_HANDLE:
	case STATUS_OBJECT_TYPE_MISMATCH:
		return WSAENOTSOCK;

	case STATUS_INSUFFICIENT_RESOURCES:
	case STATUS_PAGEFILE_QUOTA:
	case STATUS_COMMITMENT_LIMIT:
	case STATUS_WORKING_SET_QUOTA:
	case STATUS_NO_MEMORY:
	case STATUS_QUOTA_EXCEEDED:
	case STATUS_TOO_MANY_PAGING_FILES:
	case STATUS_REMOTE_RESOURCES:
		return WSAENOBUFS;

	case STATUS_TOO_MANY_ADDRESSES:
	case STATUS_SHARING_VIOLATION:
	case STATUS_ADDRESS_ALREADY_EXISTS:
		return WSAEADDRINUSE;

	case STATUS_LINK_TIMEOUT:
	case STATUS_IO_TIMEOUT:
	case STATUS_TIMEOUT:
		return WSAETIMEDOUT;

	case STATUS_GRACEFUL_DISCONNECT:
		return WSAEDISCON;

	case STATUS_REMOTE_DISCONNECT:
	case STATUS_CONNECTION_RESET:
	case STATUS_LINK_FAILED:
	case STATUS_CONNECTION_DISCONNECTED:
	case STATUS_PORT_UNREACHABLE:
	case STATUS_HOPLIMIT_EXCEEDED:
		return WSAECONNRESET;

	case STATUS_LOCAL_DISCONNECT:
	case STATUS_TRANSACTION_ABORTED:
	case STATUS_CONNECTION_ABORTED:
		return WSAECONNABORTED;

	case STATUS_BAD_NETWORK_PATH:
	case STATUS_NETWORK_UNREACHABLE:
	case STATUS_PROTOCOL_UNREACHABLE:
		return WSAENETUNREACH;

	case STATUS_HOST_UNREACHABLE:
		return WSAEHOSTUNREACH;

	case STATUS_CANCELLED:
	case STATUS_REQUEST_ABORTED:
		return WSAEINTR;

	case STATUS_BUFFER_OVERFLOW:
	case STATUS_INVALID_BUFFER_SIZE:
		return WSAEMSGSIZE;

	case STATUS_BUFFER_TOO_SMALL:
	case STATUS_ACCESS_VIOLATION:
		return WSAEFAULT;

	case STATUS_DEVICE_NOT_READY:
	case STATUS_REQUEST_NOT_ACCEPTED:
		return WSAEWOULDBLOCK;

	case STATUS_INVALID_NETWORK_RESPONSE:
	case STATUS_NETWORK_BUSY:
	case STATUS_NO_SUCH_DEVICE:
	case STATUS_NO_SUCH_FILE:
	case STATUS_OBJECT_PATH_NOT_FOUND:
	case STATUS_OBJECT_NAME_NOT_FOUND:
	case STATUS_UNEXPECTED_NETWORK_ERROR:
		return WSAENETDOWN;

	case STATUS_INVALID_CONNECTION:
		return WSAENOTCONN;

	case STATUS_REMOTE_NOT_LISTENING:
	case STATUS_CONNECTION_REFUSED:
		return WSAECONNREFUSED;

	case STATUS_PIPE_DISCONNECTED:
		return WSAESHUTDOWN;

	case STATUS_CONFLICTING_ADDRESSES:
	case STATUS_INVALID_ADDRESS:
	case STATUS_INVALID_ADDRESS_COMPONENT:
		return WSAEADDRNOTAVAIL;

	case STATUS_NOT_SUPPORTED:
	case STATUS_NOT_IMPLEMENTED:
		return WSAEOPNOTSUPP;

	case STATUS_ACCESS_DENIED:
		return WSAEACCES;

	default:
		if ((Status & (FACILITY_NTWIN32 << 16)) == (FACILITY_NTWIN32 << 16) &&
			(Status & (ERROR_SEVERITY_ERROR | ERROR_SEVERITY_WARNING))) {
			/* It's a windows error that has been previously mapped to an */
			/* ntstatus code. */
			return (DWORD)(Status & 0xffff);
		}
		else {
			/* The default fallback for unmappable ntstatus codes. */
			return WSAEINVAL;
		}
	}
}

/**
 * ��ȡ��̬����
 * @param	LibName		ģ����
 * @param	RoutineName	������
 */
PVOID
CoGetDynamicImportRoutine(
	LPSTR LibName,
	LPSTR RoutineName
) {

	HMODULE Module = GetModuleHandleA(LibName);
	if (Module == NULL)
		return NULL;

	return GetProcAddress(Module, RoutineName);
}

/**
 * �̺߳���ִ������
 */
VOID
WINAPI CoCompatRoutineHost(
	LPVOID lpFiberParameter
) {

	DWORD StackSize;
	//��ȡ����
	PCOROUTINE_COMPAT_CALL CompatCall = (PCOROUTINE_COMPAT_CALL)lpFiberParameter;

	__try {
		CompatCall->CompatRoutine(CompatCall->Parameter);
	}
	__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW) {

		//��ȡ��ǰջ��С������
		if (SetThreadStackGuarantee(&StackSize)) {
			StackSize += Co_SystemPageSize;
			SetThreadStackGuarantee(&StackSize);
			_resetstkoflw();
		}
	}

	free(CompatCall);

	CoYield(TRUE);
}

/**
 * �˳̺���ִ������
 */
VOID
WINAPI CoStandardRoutineHost(
	LPVOID lpFiberParameter
) {

	DWORD StackSize;
	//��ȡ����
	PCOROUTINE_STANDARD_CALL StandardCall = (PCOROUTINE_STANDARD_CALL)lpFiberParameter;

	__try {
		StandardCall->FiberRoutine(StandardCall->Parameter);
	}
	__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW) {

		//��ȡ��ǰջ��С������
		if (SetThreadStackGuarantee(&StackSize)) {
			StackSize += Co_SystemPageSize;
			SetThreadStackGuarantee(&StackSize);
			_resetstkoflw();
		}
	}

	free(StandardCall);

	CoYield(TRUE);
}

/**
 * �ֶ�����Э�̵���
 */
VOID
CoYield(
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
	DWORD TransBytes;
	DWORD Flags;

	//��TLS�л�ȡЭ��ʵ��
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

DEAL_COMPLETED_IO:
	//���ȴ�����ʱִ�ж���
	while (!Instance->DelayExecutionList->empty()) {

		//ȡ����һ���¼����ж��Ƿ�С�ڵ��ڵ�ǰʱ�䣬�ǵĻ�˵������
		PCOROUTINE_EXECUTE_DELAY ExecuteDelay = Instance->DelayExecutionList->front();
		if (ExecuteDelay->TimeAtLeast > GetTickCount64())
			break;

		//������һ���¼���ִ��֮
		Instance->DelayExecutionList->pop_front();
		SwitchToFiber(ExecuteDelay->Fiber);

		//���ִ�����
		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}
	}

	//�����Iocp������¼�
	while (GetQueuedCompletionStatus(Instance->Iocp, &ByteTransfered, &IoContext, &Overlapped, Timeout)) {
		
		PCOROUTINE_OVERLAPPED_WARPPER Context = (PCOROUTINE_OVERLAPPED_WARPPER)
			CONTAINING_RECORD(Overlapped, COROUTINE_OVERLAPPED_WARPPER, Overlapped);
		Context->BytesTransfered = ByteTransfered;
		
		//libuv��������������
		if (Context->AsioType == ASIO_FILE) {
			if (RtlNtStatusToDosError == NULL)
				RtlNtStatusToDosError = (LpfnRtlNtStatusToDosError)CoGetDynamicImportRoutine("ntdll.dll", "RtlNtStatusToDosError");
			Context->ErrorCode = RtlNtStatusToDosError((NTSTATUS)Context->Overlapped.Internal);
		}
		else if (Context->AsioType == ASIO_NET) {
			Context->ErrorCode = CoNtatusToWinsockError((NTSTATUS)Context->Overlapped.Internal);
		}

		//����ṹ������Э��ִ���б��ͷ���
		Victim = Context->Fiber;
		SwitchToFiber(Victim);

		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}
	}

	//����ִ����Ϊ����ԭ���ϵ�Э�̻����µ�Э��
	if (!Instance->FiberList->empty()) {

		Victim = (PVOID)Instance->FiberList->front();
		Instance->FiberList->pop_front();
		SwitchToFiber(Victim);
		
		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}

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
 * ��ʵ�ϵ��Է��֣�StackSize����ָ�����٣�ֻҪջ�ռ䲻����ϵͳ��
 * �Զ������µ�ջ�ռ䣬�����ᴥ���쳣���������ֵ����ȡֵ��PageSizeһ��
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
	PVOID NewFiber = CreateFiberEx(Co_SystemPageSize, StackSize, 0, StartRoutine, Parameter);
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
 * ����һ����ͨ���˳�
 * Ϊ�˱�֤�˳̶����ܼ�ʱ�Ļ��գ�������������ӿ�
 */
BOOLEAN
CoInsertStandardRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
) {

	PCOROUTINE_STANDARD_CALL StandardCall = (PCOROUTINE_STANDARD_CALL)malloc(sizeof(COROUTINE_STANDARD_CALL));
	StandardCall->FiberRoutine = StartRoutine;
	StandardCall->Parameter = Parameter;
	
	//��ջ����һ��ҳ��
	if (StackSize < Co_SystemPageSize)
		StackSize = Co_SystemPageSize;

	return CoInsertRoutine(StackSize, CoStandardRoutineHost, StandardCall, Instance);
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
	
	//����Э����ڵ�
	Instance->ScheduleRoutine = CreateFiber(Co_SystemPageSize, CoScheduleRoutine, NULL);
	if (Instance->ScheduleRoutine == NULL)
		return 0;

	Instance->FiberList->push_back(Instance->InitialRoutine);

	SwitchToFiber(Instance->ScheduleRoutine);

	ConvertFiberToThread();
	return 0;
}

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
) {

	//��TLS�л�ȡЭ��ʵ��
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	//����һ����ʱִ�ж���
	PCOROUTINE_EXECUTE_DELAY DelayExecution = (PCOROUTINE_EXECUTE_DELAY)malloc(sizeof(COROUTINE_EXECUTE_DELAY));
	DelayExecution->Fiber = Fiber;
	DelayExecution->TimeAtLeast = GetTickCount64() + MillionSecond;

	//�����ʱ����Ϊ��
	if (Instance->DelayExecutionList->empty()) {
		Instance->DelayExecutionList->push_front(DelayExecution);
		return;
	}

	//�������С��ʱ��ҪС
	if (DelayExecution->TimeAtLeast < Instance->DelayExecutionList->front()->TimeAtLeast) {
		Instance->DelayExecutionList->push_front(DelayExecution);
		return;
	}

	//���뵽��Ӧ��λ����
	BOOLEAN Inserted = FALSE;
	list<PCOROUTINE_EXECUTE_DELAY>::iterator Iter;
	for (Iter = Instance->DelayExecutionList->begin();Iter != Instance->DelayExecutionList->end();Iter++) {
		PCOROUTINE_EXECUTE_DELAY Node = *Iter;
		if (Node->TimeAtLeast < DelayExecution->TimeAtLeast) {
			continue;
		}
		break;
	}
	Instance->DelayExecutionList->insert(Iter--, DelayExecution);

	return;
}

/**
 * �ֶ���������
 */
BOOLEAN 
CoStartCoroutineManually(
) {

	//��TLS�л�ȡЭ��ʵ��
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	if (Instance == NULL)
		return FALSE;

	if (ConvertThreadToFiber(NULL) == NULL)
		return FALSE;

	SwitchToFiber(Instance->ScheduleRoutine);

	//��������
	return TRUE;
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

	if (Instance == NULL)
		return NULL;
	memset(Instance, 0, sizeof(Instance));

	//�����߳���ص�IO��ɶ˿�
	Instance->Iocp= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Instance->Iocp == NULL)
		goto ERROR_EXIT;

	//����Э�̵��ȶ���
	Instance->FiberList = new list<void*>;
	Instance->DelayExecutionList = new list<PCOROUTINE_EXECUTE_DELAY>;
	Instance->InitialRoutine = CreateFiberEx(StackSize, 0x1000, 0, InitRoutine, Parameter);
	if (Instance->InitialRoutine == NULL)
		goto ERROR_EXIT;

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

		//����Э����ڵ�
		Instance->ScheduleRoutine = CreateFiber(Co_SystemPageSize, CoScheduleRoutine, NULL);
		if (Instance->ScheduleRoutine == NULL)
			goto ERROR_EXIT;

		Instance->FiberList->push_back(Instance->InitialRoutine);
	}

	return (HANDLE)Instance;

ERROR_EXIT:
	if (Instance->Iocp)
		CloseHandle(Instance->Iocp);

	if (Instance->InitialRoutine)
		DeleteFiber(Instance->InitialRoutine);

	free(Instance);
	return NULL;
}

/**
 * ��ʼ��Э�̿�
 */
VOID
CoInitialize(
) {

	return;
}