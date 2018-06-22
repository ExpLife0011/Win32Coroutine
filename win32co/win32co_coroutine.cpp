// win32_coroutine.cpp : �������̨Ӧ�ó������ڵ㡣
//

#define UMDF_USING_NTSTATUS

#include <ntstatus.h>
#include <intrin.h>

#include "win32co_coroutine.h"
#include "win32co_hook.h"
#include "win32co_sysroutine.h"
#include "win32co_list.h"
#include "win32co_error.h"

typedef DWORD NTSTATUS;

//Coroutine--->Parameter------->Fiber
//		   --->MessageQueue	 |->FiberParameter

//ִ�����ȼ�=�¼���׼���ȼ�+ʱ�����+�˳����ȼ�

#define PRIORITY_BASE_TIMER			1
#define PRIORITY_BASE_IOCP			10
#define PRIORITY_BASE_NORMAL		100

#define PRIORITY_INCREMENT			1

typedef
ULONG 
(WINAPI* LpfnRtlNtStatusToDosError)(
	_In_ NTSTATUS Status
);

namespace Win32Coroutine {

	BOOLEAN Co_LibInitialized = FALSE;

	DWORD Co_SystemPageSize = 4096;
	DWORD Co_ProcessorCount;

	DWORD Co_FiberInstanceIndex;

	LpfnRtlNtStatusToDosError RtlNtStatusToDosError = NULL;

	namespace Coroutine {

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
		 * ִ�����л������
		 */
		BOOLEAN
			FORCEINLINE
			CoCheckEnvironment(
			) {

			if (Co_LibInitialized == FALSE) {
				Error::CoShowSystemError(ERROR_BAD_ENVIRONMENT,
					NULL,
					Error::ERROR_LEVEL::Error
				);
				return FALSE;
			}

			return TRUE;
		}

		/** 
		 * ��ȡЭ��ʵ��
		 */
		PCOROUTINE_INSTANCE
			FORCEINLINE
			CoGetCoInstance(
			) {
			
			return (PCOROUTINE_INSTANCE)TlsGetValue(0);
		}

		/**
		 * ��ȡFiberʵ��
		 */
		PCOROUTINE_FIBER_INSTANCE
			FORCEINLINE
			CoGetFiInstance(
			) {

			return (PCOROUTINE_FIBER_INSTANCE)FlsGetValue(0);
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
		 * ��ִ���˳�ǰ���ʼ��
		 */
		COEXPORT
		VOID
			WINAPI
			CoPreInitialize(
				LPVOID lpParameter
			) {

			STORE_FIBER_INSTANCE(lpParameter);
		}

		//֮�����ṩ����������������Ϊ��ִ��CoYeild(TRUE)��ʱ������Fiber����
		//����ԭʼ������Ҫ�ֶ����е��ȣ�����ƻ������ֵ����Ŀ��

		//����û�ϣ���ֶ����Ƚ����˳̣��Ͳ�Ҫʹ����������

		/**
		 * �̺߳���ִ������
		 */
		VOID
			WINAPI
			CoCompatRoutineHost(
				LPVOID lpFiberParameter
			) {

			DWORD StackSize;
			//��ȡ����
			PCOROUTINE_FIBER_INSTANCE FiInstance = (PCOROUTINE_FIBER_INSTANCE)lpFiberParameter;
			CoPreInitialize(lpFiberParameter);

			__try {
				FiInstance->UserCompatRoutine(FiInstance->UserParameter);
			}
			__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW) {

				Error::CoShowSystemError(ERROR_STACK_OVERFLOW, NULL, Error::ERROR_LEVEL::Warning);

				//��ȡ��ǰջ��С������
				if (SetThreadStackGuarantee(&StackSize)) {
					StackSize += Co_SystemPageSize;
					SetThreadStackGuarantee(&StackSize);
					_resetstkoflw();
				}
			}

			CoYield(TRUE);
		}

		/**
		 * �˳̺���ִ������
		 */
		VOID
			WINAPI
			CoStandardRoutineHost(
				LPVOID lpFiberParameter
			) {

			DWORD StackSize;
			//��ȡ����
			PCOROUTINE_FIBER_INSTANCE FiInstance = (PCOROUTINE_FIBER_INSTANCE)lpFiberParameter;
			CoPreInitialize(lpFiberParameter);

			__try {
				FiInstance->UserStanderRoutine(FiInstance->UserParameter);
			}
			__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW) {

				Error::CoShowSystemError(ERROR_STACK_OVERFLOW, NULL, Error::ERROR_LEVEL::Warning);

				//��ȡ��ǰջ��С������
				if (SetThreadStackGuarantee(&StackSize)) {
					StackSize += Co_SystemPageSize;
					SetThreadStackGuarantee(&StackSize);
					_resetstkoflw();
				}
			}

			CoYield(TRUE);
		}

		COEXPORT
		VOID
			WINAPI
			CoDeleteFiberInstance(
				PCOROUTINE_FIBER_INSTANCE FiInstance
			) {

		}

		/**
		 * �ֶ�����Э�̵���
		 */
		COEXPORT
		VOID
			WINAPI
			CoYield(
				BOOLEAN Terminate
			) {

			PCOROUTINE_INSTANCE Instance = CoGetCoInstance();
			Instance->LastFiberFinished = Terminate;

			//��������������������ʵ������
			if (Terminate) {
				free(CoGetFiInstance());
			}

			SwitchToFiber(GET_FIBER_FROM_INSTANCE(Instance->ScheduleRoutine));
		}

#define CHECK_FINISH(Fiber)									\
		if (Instance->LastFiberFinished == TRUE) {			\
			DeleteFiber(Fiber);								\
			Instance->LastFiberFinished = FALSE;			\
		}													\

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
			PCOROUTINE_FIBER_INSTANCE FiInstance;
			PCOROUTINE_INSTANCE Instance = CoGetCoInstance();
			if (Instance == NULL) {
				Error::CoShowCoroutineError("Э��ʵ����ȡʧ��.", 
					"CoScheduleRoutine", 
					Error::ERROR_LEVEL::FatalError
				);
			}

		DEAL_COMPLETED_IO:
			//���ȴ�����ʱִ�ж���
			while (!IsListEmpty(&Instance->DelayExecutionList)) {

				//ȡ����һ���¼����ж��Ƿ�С�ڵ��ڵ�ǰʱ�䣬�ǵĻ�˵������
				PCOROUTINE_EXECUTE_DELAY ExecuteDelay = (PCOROUTINE_EXECUTE_DELAY)Instance->DelayExecutionList.Flink;
				if (ExecuteDelay->TimeAtLeast > GetTickCount64())
					break;

				//������һ���¼���ִ��֮
				RemoveHeadList(&Instance->DelayExecutionList);
				free(ExecuteDelay);

				SwitchToFiber(ExecuteDelay->Fiber);

				//���ִ�����
				CHECK_FINISH(ExecuteDelay->Fiber);
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

				CHECK_FINISH(Victim);
			}
			
			//����ִ����Ϊ����ԭ���ϵ�Э�̻����µ�Э��
			if (!IsListEmpty(&Instance->FiberInstanceList)) {

				//��ȡ��һ��Instance
				FiInstance = (PCOROUTINE_FIBER_INSTANCE)RemoveHeadList(&Instance->FiberInstanceList);

				//��ת��Instance
				Victim = FiInstance->FiberRoutine;
				SwitchToFiber(Victim);

				//����û�����������Fiber��ص��ڴ�
				CHECK_FINISH(Victim);

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
		 * �Զ������µ�ջ�ռ䣬�����ᴥ���쳣���������ֵ����ȡֵ��PageSize
		 */
		COEXPORT
		PCOROUTINE_FIBER_INSTANCE
			WINAPI
			CoCreateFiberInstance(
				SIZE_T StackSize,
				LPFIBER_START_ROUTINE StartRoutine,
				LPVOID Parameter
			) {

			//�����˳�ʵ��
			PCOROUTINE_FIBER_INSTANCE FiInstance;
			FiInstance = (PCOROUTINE_FIBER_INSTANCE)malloc(sizeof(COROUTINE_FIBER_INSTANCE));
			if (FiInstance == NULL) {
				Error::CoError_LowMemory();
				return NULL;
			}

			//����һ���˳̣��˳�ʵ����Ϊ����
			PVOID NewFiber = CreateFiberEx(Co_SystemPageSize, StackSize, 0, StartRoutine, FiInstance);
			if (NewFiber == NULL) {
				Error::CoShowSystemError(GetLastError(),
					"CreateFiberEx",
					Error::ERROR_LEVEL::Error
				);
				return NULL;
			}

			//�˳̲���
			if (StartRoutine == CoCompatRoutineHost || StartRoutine == CoStandardRoutineHost)
				FiInstance->UserParameter = FiInstance;
			else
				FiInstance->UserParameter = Parameter;
			//��������
			FiInstance->FiberRoutine = NewFiber;

			return FiInstance;
		}

#define GET_COROUTINE_INST(_inst_)		(((_inst_)==NULL)?TlsGetValue(0):(_inst_))

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
			) {

			//��ȡЭ��ʵ��
			PCOROUTINE_INSTANCE CoInstance = (PCOROUTINE_INSTANCE)GET_COROUTINE_INST(Instance);
			if (CoInstance == NULL) {
				Error::CoShowCoroutineError("Э��ʵ����ȡʧ��.", 
					NULL, 
					Error::ERROR_LEVEL::Error
				);
				return NULL;
			}

			//��ջ����һ��ҳ��
			if (StackSize < Co_SystemPageSize)
				StackSize = Co_SystemPageSize;

			//�����˳�ʵ��
			PCOROUTINE_FIBER_INSTANCE FiInstance = CoCreateFiberInstance(StackSize, CoCompatRoutineHost, NULL);
			if (FiInstance == NULL) {
				Error::CoShowCoroutineError("�����˳�ʵ��ʧ��.", 
					"CoInsertCompatRoutine", 
					Error::ERROR_LEVEL::Error
				);
				return NULL;
			}

			FiInstance->UserCompatRoutine = StartRoutine;
			FiInstance->UserParameter = Parameter;

			//������˳�ʵ������
			InsertTailList(&Instance->FiberInstanceList, &FiInstance->Entry);

			return (HANDLE)FiInstance;
		}

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
			) {

			//��ȡЭ��ʵ��
			PCOROUTINE_INSTANCE CoInstance = (PCOROUTINE_INSTANCE)GET_COROUTINE_INST(Instance);
			if (CoInstance == NULL) {
				Error::CoShowCoroutineError("Э��ʵ����ȡʧ��.", 
					NULL,
					Error::ERROR_LEVEL::Error
				);
				return NULL;
			}

			//��ջ����һ��ҳ��
			if (StackSize < Co_SystemPageSize)
				StackSize = Co_SystemPageSize;

			//�����˳�ʵ��
			PCOROUTINE_FIBER_INSTANCE FiInstance = CoCreateFiberInstance(StackSize, CoStandardRoutineHost, NULL);
			if (FiInstance == NULL) {
				Error::CoShowCoroutineError("�����˳�ʵ��ʧ��.",
					"CoInsertStandardRoutine",
					Error::ERROR_LEVEL::Error
				);
				return NULL;
			}

			FiInstance->UserStanderRoutine = StartRoutine;
			FiInstance->UserParameter = Parameter;

			//������˳�ʵ������
			InsertTailList(&CoInstance->FiberInstanceList, &FiInstance->Entry);

			return (HANDLE)FiInstance;
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

			Instance->HostThread = ConvertThreadToFiber(NULL);
			if (Instance->HostThread == NULL) {
				Error::CoShowSystemError(GetLastError(), 
					"ConvertThreadToFiber", 
					Error::ERROR_LEVEL::Error
				);
				goto EXIT;
			}

			if (TlsSetValue(0, Instance) == FALSE) {
				Error::CoShowSystemError(GetLastError(), 
					"TlsSetValue",
					Error::ERROR_LEVEL::Error
				);
				goto REVERT;
			}

			//����Э����ڵ�-�����˳�
			Instance->ScheduleRoutine = CoCreateFiberInstance(0x1000, CoScheduleRoutine, Instance);
			if (Instance->ScheduleRoutine == NULL) {
				Error::CoShowCoroutineError("���������˳�ʧ��.", 
					"CoThreadEntryPoint", 
					Error::ERROR_LEVEL::Error
				);
				goto REVERT;
			}

			//�л��������˳�
			SwitchToFiber(GET_FIBER_FROM_INSTANCE(Instance->ScheduleRoutine));

			//�ӵ����˳��˳�
REVERT:
			ConvertFiberToThread();

EXIT:
			free(Instance);

			return 0;
		}

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
				LPVOID Fiber,
				DWORD MillionSecond
			) {

			//��TLS�л�ȡЭ��ʵ��
			PCOROUTINE_INSTANCE Instance = CoGetCoInstance();
			PCOROUTINE_EXECUTE_DELAY Entry;

			//����һ����ʱִ�ж���
			PCOROUTINE_EXECUTE_DELAY DelayExecution = (PCOROUTINE_EXECUTE_DELAY)malloc(sizeof(COROUTINE_EXECUTE_DELAY));
			if (DelayExecution == NULL) {
				Error::CoError_LowMemory();
				return FALSE;
			}

			DelayExecution->Fiber = Fiber;
			DelayExecution->TimeAtLeast = GetTickCount64() + MillionSecond;

			//�����ʱ����Ϊ��
			if (IsListEmpty(&Instance->DelayExecutionList)) {
				InsertTailList(&Instance->DelayExecutionList, &DelayExecution->Entry);
				return TRUE;
			}

			//�������С��ʱ��ҪС
			if (DelayExecution->TimeAtLeast < ((PCOROUTINE_EXECUTE_DELAY)Instance->DelayExecutionList.Flink)->TimeAtLeast) {
				InsertTailList(&Instance->DelayExecutionList, &DelayExecution->Entry);
				return TRUE;
			}

			//���뵽��Ӧ��λ����
			BOOLEAN Inserted = FALSE;

			for (Entry = (PCOROUTINE_EXECUTE_DELAY)Instance->DelayExecutionList.Flink;
				Entry != (PCOROUTINE_EXECUTE_DELAY)&Instance->DelayExecutionList;
				Entry = (PCOROUTINE_EXECUTE_DELAY)Entry->Entry.Flink) {
				if (Entry->TimeAtLeast < DelayExecution->TimeAtLeast) {
					continue;
				}
				break;
			}
			InsertTailList(Entry->Entry.Blink, (PLIST_ENTRY)&DelayExecution);

			return TRUE;
		}

		/**
		 * �ֶ���������
		 */
		COEXPORT
		BOOLEAN
			WINAPI
			CoStartCoroutineManually(
			) {

			//��TLS�л�ȡЭ��ʵ��
			PCOROUTINE_INSTANCE Instance = CoGetCoInstance();
			if (Instance == NULL)
				return FALSE;

			if (ConvertThreadToFiber(NULL) == NULL)
				return FALSE;

			SwitchToFiber(GET_FIBER_FROM_INSTANCE(Instance->ScheduleRoutine));

			//��������
			return TRUE;
		}

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
			) {

			if (!CoCheckEnvironment()) {
				return NULL;
			}

			PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)malloc(sizeof(COROUTINE_INSTANCE));
			DWORD ThreadId;

			if (Instance == NULL) {
				Error::CoError_LowMemory();
				return NULL;
			}

			memset(Instance, 0, sizeof(Instance));

			//�����߳���ص�IO��ɶ˿�
			Instance->Iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
			if (Instance->Iocp == NULL) {
				Error::CoShowSystemError(GetLastError(), 
					"CreateIoCompletionPort", 
					Error::ERROR_LEVEL::Error
				);
				goto ERROR_EXIT;
			}

			//����Э�̵��ȶ���
			InitializeListHead(&Instance->FiberInstanceList);
			InitializeListHead(&Instance->DelayExecutionList);
			if (InitRoutine) {
				Instance->InitialRoutine = CoInsertStandardRoutine(0x1000, InitRoutine, Parameter, Instance);
				if (Instance->InitialRoutine == NULL) {
					Error::CoShowCoroutineError("������ں���ʧ��.", 
						"CoCreateCoroutine",
						Error::ERROR_LEVEL::Error
					);
					goto ERROR_EXIT;
				}
			}

			if (NewThread) {

				//����Э�������߳�
				Instance->ThreadHandle = CreateThread(NULL, 0, CoThreadEntryPoint, Instance, 0, &ThreadId);
				if (Instance->ThreadHandle == NULL) {
					Error::CoShowSystemError(GetLastError(),
						"CreateThread",
						Error::ERROR_LEVEL::Error
					);
					goto ERROR_EXIT;
				}
			}
			else {

				//������������̣߳�����ֻ�ǳ�ʼ������Ҫ�ֶ���ʼ����
				TlsSetValue(0, Instance);

				//����Э����ڵ�
				Instance->ScheduleRoutine = CoCreateFiberInstance(0x1000, CoScheduleRoutine, NULL);
				if (Instance->ScheduleRoutine == NULL)
					goto ERROR_EXIT;

				InsertTailList(&Instance->FiberInstanceList, (PLIST_ENTRY)Instance->InitialRoutine);
			}

			return (HANDLE)Instance;

		ERROR_EXIT:
			if (Instance->Iocp)
				CloseHandle(Instance->Iocp);

			if (Instance->InitialRoutine)
				DeleteFiber(GET_FIBER_FROM_INSTANCE(Instance->InitialRoutine));

			free(Instance);
			return NULL;
		}

		/**
		 * ��ʼ��Э�̿�
		 */
		COEXPORT
		VOID
			WINAPI
			CoInitialize(
			) {

			SYSTEM_INFO SystemInfo;
			GetSystemInfo(&SystemInfo);

			Co_SystemPageSize = SystemInfo.dwPageSize;
			Co_ProcessorCount = SystemInfo.dwNumberOfProcessors;

			Co_FiberInstanceIndex = FlsAlloc(NULL);

			Co_LibInitialized = TRUE;

			return;
		}

	}

}