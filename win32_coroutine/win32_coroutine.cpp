// win32_coroutine.cpp : 定义控制台应用程序的入口点。
//

#define UMDF_USING_NTSTATUS

#include "stdafx.h"
#include "windows.h"
#include <ntstatus.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"
#include "win32_coroutine.h"
#include "win32_list.h"

typedef DWORD NTSTATUS;

using namespace std;

//Coroutine--->Parameter------->Fiber
//		   --->MessageQueue	 |->FiberParameter

//执行优先级=事件基准优先级+时间基数+纤程优先级

#define PRIORITY_BASE_TIMER			1
#define PRIORITY_BASE_IOCP			10
#define PRIORITY_BASE_NORMAL		100

#define PRIORITY_INCREMENT			1

DWORD Co_SystemPageSize = 4096;
DWORD Co_ProcessorCount;

DWORD Co_FiberInstanceIndex;

typedef
ULONG 
(WINAPI* LpfnRtlNtStatusToDosError)(
	_In_ NTSTATUS Status
);

LpfnRtlNtStatusToDosError RtlNtStatusToDosError = NULL;

/**
 * 将NT错误码转换为Winsock错误码
 * 从libuv抄过来的
 * @param	status	Nt错误码
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
 * 获取动态导入
 * @param	LibName		模块名
 * @param	RoutineName	函数名
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
 * 在执行纤程前域初始化
 */
VOID
CoPreInitialize(
	LPVOID lpParameter
) {

	STORE_FIBER_INSTANCE(lpParameter);
}

//之所以提供两个宿主函数，是为了执行CoYeild(TRUE)后及时的销毁Fiber对象
//否则原始代码需要手动进行调度，这会破坏非入侵的设计目标

//如果用户希望手动调度结束纤程，就不要使用宿主函数

/**
 * 线程函数执行宿主
 */
VOID
WINAPI CoCompatRoutineHost(
	LPVOID lpFiberParameter
) {

	DWORD StackSize;
	//获取参数
	PCOROUTINE_FIBER_INSTANCE FiInstance = (PCOROUTINE_FIBER_INSTANCE)lpFiberParameter;
	CoPreInitialize(lpFiberParameter);

	__try {
		FiInstance->UserCompatRoutine(FiInstance->UserParameter);
	}
	__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW) {

		//获取当前栈大小并调整
		if (SetThreadStackGuarantee(&StackSize)) {
			StackSize += Co_SystemPageSize;
			SetThreadStackGuarantee(&StackSize);
			_resetstkoflw();
		}
	}

	CoYield(TRUE);
}

/**
 * 纤程函数执行宿主
 */
VOID
WINAPI CoStandardRoutineHost(
	LPVOID lpFiberParameter
) {

	DWORD StackSize;
	//获取参数
	PCOROUTINE_FIBER_INSTANCE FiInstance = (PCOROUTINE_FIBER_INSTANCE)lpFiberParameter;
	CoPreInitialize(lpFiberParameter);

	__try {
		FiInstance->UserStanderRoutine(FiInstance->UserParameter);
	}
	__except (GetExceptionCode() == EXCEPTION_STACK_OVERFLOW) {

		//获取当前栈大小并调整
		if (SetThreadStackGuarantee(&StackSize)) {
			StackSize += Co_SystemPageSize;
			SetThreadStackGuarantee(&StackSize);
			_resetstkoflw();
		}
	}

	CoYield(TRUE);
}

/**
 * 手动进行协程调度
 */
VOID
CoYield(
	BOOLEAN Terminate
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	Instance->LastFiberFinished = Terminate;

	//如果结束，在这里就销毁实例参数
	if (Terminate) {
		free(RETRIEVE_FIBER_INSTANCE());
	}

	SwitchToFiber(GET_FIBER_FROM_INSTANCE(Instance->ScheduleRoutine));
}

/**
 * 调度协程
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

	//从TLS中获取协程实例
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	PCOROUTINE_FIBER_INSTANCE FiInstance;

DEAL_COMPLETED_IO:
	//首先处理延时执行队列
	while (!IsListEmpty(&Instance->DelayExecutionList)) {

		//取出第一个事件，判断是否小于等于当前时间，是的话说明命中
		PCOROUTINE_EXECUTE_DELAY ExecuteDelay = (PCOROUTINE_EXECUTE_DELAY)Instance->DelayExecutionList.Flink;
		if (ExecuteDelay->TimeAtLeast > GetTickCount64())
			break;

		//弹出第一个事件并执行之
		RemoveHeadList(&Instance->DelayExecutionList);
		free(ExecuteDelay);

		SwitchToFiber(ExecuteDelay->Fiber);

		//如果执行完毕
		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(ExecuteDelay->Fiber);
			Instance->LastFiberFinished = FALSE;
		}
	}

	//随后处理Iocp的完成事件
	while (GetQueuedCompletionStatus(Instance->Iocp, &ByteTransfered, &IoContext, &Overlapped, Timeout)) {
		
		PCOROUTINE_OVERLAPPED_WARPPER Context = (PCOROUTINE_OVERLAPPED_WARPPER)
			CONTAINING_RECORD(Overlapped, COROUTINE_OVERLAPPED_WARPPER, Overlapped);
		Context->BytesTransfered = ByteTransfered;
		
		//libuv是这样处理错误的
		if (Context->AsioType == ASIO_FILE) {
			if (RtlNtStatusToDosError == NULL)
				RtlNtStatusToDosError = (LpfnRtlNtStatusToDosError)CoGetDynamicImportRoutine("ntdll.dll", "RtlNtStatusToDosError");
			Context->ErrorCode = RtlNtStatusToDosError((NTSTATUS)Context->Overlapped.Internal);
		}
		else if (Context->AsioType == ASIO_NET) {
			Context->ErrorCode = CoNtatusToWinsockError((NTSTATUS)Context->Overlapped.Internal);
		}

		//这个结构可能在协程执行中被释放了
		Victim = Context->Fiber;
		SwitchToFiber(Victim);

		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}
	}

	//继续执行因为其他原因打断的协程或者新的协程
	if (!IsListEmpty(&Instance->FiberInstanceList)) {

		//获取第一个Instance
		FiInstance = (PCOROUTINE_FIBER_INSTANCE)RemoveHeadList(&Instance->FiberInstanceList);

		//跳转到Instance
		Victim = FiInstance->FiberRoutine;
		SwitchToFiber(Victim);
		
		//如果用户结束，销毁Fiber相关的内存
		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}

		//如果有协程可执行，那么可能后面还有新的协程等待执行
		Timeout = 0;
	}
	else {

		//如果没有，那么就让完成端口等久一点
		Timeout = 500;
	}

	goto DEAL_COMPLETED_IO;
}

/**
 * 创建一个协程
 * 事实上调试发现，StackSize无论指定多少，只要栈空间不够，系统会
 * 自动申请新的栈空间，而不会触发异常，所以这个值建议取值和PageSize
 */
PCOROUTINE_FIBER_INSTANCE
CoCreateFiberInstance(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter
) {

	//分配纤程实例
	PCOROUTINE_FIBER_INSTANCE FiInstance;
	FiInstance = (PCOROUTINE_FIBER_INSTANCE)malloc(sizeof(COROUTINE_FIBER_INSTANCE));
	if (FiInstance == NULL)
		return NULL;

	//创建一个纤程，纤程实例作为参数
	PVOID NewFiber = CreateFiberEx(Co_SystemPageSize, StackSize, 0, StartRoutine, FiInstance);
	if (NewFiber == NULL)
		return NULL;

	//纤程参数
	if (StartRoutine == CoCompatRoutineHost || StartRoutine == CoStandardRoutineHost)
		Parameter = FiInstance;
	else
		Parameter = Parameter;
	//启动函数
	FiInstance->FiberRoutine = NewFiber;

	return FiInstance;
}

#define GET_COROUTINE_INST(_inst_)		(((_inst_)==NULL)?TlsGetValue(0):(_inst_))

/**
 * 创建一个兼容线程ABI的协程
 */
HANDLE
CoInsertCompatRoutine(
	SIZE_T StackSize,
	LPTHREAD_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
) {

	//获取协程实例
	PCOROUTINE_INSTANCE CoInstance = (PCOROUTINE_INSTANCE)GET_COROUTINE_INST(Instance);
	if (CoInstance == NULL)
		return NULL;

	//堆栈至少一个页面
	if (StackSize < Co_SystemPageSize)
		StackSize = Co_SystemPageSize;

	//创建纤程实例
	PCOROUTINE_FIBER_INSTANCE FiInstance = CoCreateFiberInstance(StackSize, CoCompatRoutineHost, NULL);
	if (FiInstance == NULL)
		return NULL;

	FiInstance->UserCompatRoutine = StartRoutine;
	FiInstance->UserParameter = Parameter;

	//保存进纤程实例队列
	InsertTailList(&Instance->FiberInstanceList, &FiInstance->Entry);

	return (HANDLE)FiInstance;
}

/**
 * 创建一个普通的纤程
 * 为了保证纤程对象能及时的回收，尽量调用这个接口
 */
HANDLE
CoInsertStandardRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
) {

	//获取协程实例
	PCOROUTINE_INSTANCE CoInstance = (PCOROUTINE_INSTANCE)GET_COROUTINE_INST(Instance);
	if (CoInstance == NULL)
		return NULL;

	//堆栈至少一个页面
	if (StackSize < Co_SystemPageSize)
		StackSize = Co_SystemPageSize;

	//创建纤程实例
	PCOROUTINE_FIBER_INSTANCE FiInstance = CoCreateFiberInstance(StackSize, CoStandardRoutineHost, NULL);
	if (FiInstance == NULL)
		return NULL;

	FiInstance->UserStanderRoutine = StartRoutine;
	FiInstance->UserParameter = Parameter;

	//保存进纤程实例队列
	InsertTailList(&Instance->FiberInstanceList, &FiInstance->Entry);

	return (HANDLE)FiInstance;
}

/**
 * 纤程入口点
 */
DWORD
WINAPI 
CoThreadEntryPoint(
	LPVOID lpThreadParameter
) {

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)lpThreadParameter;

	Instance->HostThread = ConvertThreadToFiber(NULL);
	
	//创建协程入口点-调度纤程
	Instance->ScheduleRoutine = CoCreateFiberInstance(0x1000, CoScheduleRoutine, NULL);
	if (Instance->ScheduleRoutine == NULL)
		return 0;

	//切换到调度纤程
	SwitchToFiber(GET_FIBER_FROM_INSTANCE(Instance->ScheduleRoutine));

	//从调度纤程退出
	ConvertFiberToThread();

	free(Instance);
	
	return 0;
}

/**
 * 添加一个延时执行事件
 * @param	Fiber			协程
 * @param	MillionSecond	延时毫秒数
 * @note	由于协程不是基于时间片调度，这个函数只能延时最小时间，往往可能会比这个时间要长
 */
BOOLEAN
CoDelayExecutionAtLeast(
	LPVOID Fiber,
	DWORD MillionSecond
) {

	//从TLS中获取协程实例
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	PCOROUTINE_EXECUTE_DELAY Entry;

	//申请一个延时执行对象
	PCOROUTINE_EXECUTE_DELAY DelayExecution = (PCOROUTINE_EXECUTE_DELAY)malloc(sizeof(COROUTINE_EXECUTE_DELAY));
	if (DelayExecution == NULL)
		return FALSE;

	DelayExecution->Fiber = Fiber;
	DelayExecution->TimeAtLeast = GetTickCount64() + MillionSecond;

	//如果延时队列为空
	if (IsListEmpty(&Instance->DelayExecutionList)) {
		InsertTailList(&Instance->DelayExecutionList, &DelayExecution->Entry);
		return TRUE;
	}

	//如果比最小延时还要小
	if (DelayExecution->TimeAtLeast < ((PCOROUTINE_EXECUTE_DELAY)Instance->DelayExecutionList.Flink)->TimeAtLeast) {
		InsertTailList(&Instance->DelayExecutionList, &DelayExecution->Entry);
		return TRUE;
	}

	//插入到对应的位置上
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
 * 手动启动调度
 */
BOOLEAN 
CoStartCoroutineManually(
) {

	//从TLS中获取协程实例
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	if (Instance == NULL)
		return FALSE;

	if (ConvertThreadToFiber(NULL) == NULL)
		return FALSE;

	SwitchToFiber(GET_FIBER_FROM_INSTANCE(Instance->ScheduleRoutine));

	//永不到达
	return TRUE;
}

/**
 * 创建一个协程
 * @param[in]	NewThread		是否创建一个新的线程
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

	//创建线程相关的IO完成端口
	Instance->Iocp= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Instance->Iocp == NULL) 
		goto ERROR_EXIT;

	//创建协程调度队列
	InitializeListHead(&Instance->FiberInstanceList);
	InitializeListHead(&Instance->DelayExecutionList);
	Instance->InitialRoutine = CoInsertStandardRoutine(0x1000, InitRoutine, Parameter, Instance);
	if (Instance->InitialRoutine == NULL)
		goto ERROR_EXIT;

	if (NewThread) {

		//创建协程宿主线程
		Instance->ThreadHandle = CreateThread(NULL, 0, CoThreadEntryPoint, Instance, 0, &ThreadId);
		if (Instance->ThreadHandle == NULL) {
			goto ERROR_EXIT;
		}
	}
	else {

		//如果不创建新线程，这里只是初始化，需要手动开始调度
		TlsSetValue(0, Instance);

		//创建协程入口点
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
		DeleteFiber(Instance->InitialRoutine);

	free(Instance);
	return NULL;
}

/**
 * 初始化协程库
 */
VOID
CoInitialize(
) {

	SYSTEM_INFO SystemInfo;
	GetSystemInfo(&SystemInfo);

	Co_SystemPageSize = SystemInfo.dwPageSize;
	Co_ProcessorCount = SystemInfo.dwNumberOfProcessors;

	Co_FiberInstanceIndex = FlsAlloc(NULL);

	return;
}