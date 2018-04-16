// win32_coroutine.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "windows.h"
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

using namespace std;

/**
 * 线程函数执行宿主
 */
VOID
WINAPI CoCompatRoutineHost(
	LPVOID lpFiberParameter
) {

	//获取参数
	PCOROUTINE_COMPAT_CALL CompatCall = (PCOROUTINE_COMPAT_CALL)lpFiberParameter;
	CompatCall->CompatRoutine(CompatCall->Parameter);

	free(CompatCall);

	CoSyncExecute(TRUE);
}

/**
 * 纤程函数执行宿主
 */
VOID
WINAPI CoStandardRoutineHost(
	LPVOID lpFiberParameter
) {

	//获取参数
	PCOROUTINE_STANDARD_CALL StandardCall = (PCOROUTINE_STANDARD_CALL)lpFiberParameter;
	StandardCall->FiberRoutine(StandardCall->Parameter);

	free(StandardCall);

	CoSyncExecute(TRUE);
}

/**
 * 手动进行协程调度
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

DEAL_COMPLETED_IO:
	//首先处理延时执行队列
	while (!Instance->DelayExecutionList->empty()) {

		//取出第一个事件，判断是否小于等于当前时间，是的话说明命中
		PCOROUTINE_EXECUTE_DELAY ExecuteDelay = Instance->DelayExecutionList->front();
		if (ExecuteDelay->TimeAtLeast > GetTickCount64())
			break;

		//弹出第一个事件并执行之
		Instance->DelayExecutionList->pop_front();
		SwitchToFiber(ExecuteDelay->Fiber);

		//如果执行完毕
		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}
	}

	//随后处理Iocp的完成事件
	while (GetQueuedCompletionStatus(Instance->Iocp, &ByteTransfered, &IoContext, &Overlapped, Timeout)) {
		
		PCOROUTINE_OVERLAPPED_WARPPER Context = (PCOROUTINE_OVERLAPPED_WARPPER)
			CONTAINING_RECORD(Overlapped, COROUTINE_OVERLAPPED_WARPPER, Overlapped);
		Context->BytesTransfered = ByteTransfered;
		Context->ErrorCode = GetLastError();

		//这个结构可能在协程执行中被释放了
		Victim = Context->Fiber;
		SwitchToFiber(Victim);

		if (Instance->LastFiberFinished == TRUE) {
			DeleteFiber(Victim);
			Instance->LastFiberFinished = FALSE;
		}
	}

	//继续执行因为其他原因打断的协程或者新的协程
	if (!Instance->FiberList->empty()) {

		Victim = (PVOID)Instance->FiberList->front();
		Instance->FiberList->pop_front();
		SwitchToFiber(Victim);
		
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

	//创建一个Coroutine
	PVOID NewFiber = CreateFiber(StackSize, StartRoutine, Parameter);
	if (NewFiber == NULL)
		return FALSE;

	//保存进Coroutine队列
	CoInstance->FiberList->push_back(NewFiber);
	
	return TRUE;
}

/**
 * 创建一个兼容线程ABI的协程
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
 * 创建一个普通的纤程
 * 为了保证纤程对象能及时的回收，尽量调用这个接口
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
	
	return CoInsertRoutine(StackSize, CoStandardRoutineHost, StandardCall, Instance);
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
 * 添加一个延时执行事件
 * @param	Fiber			协程
 * @param	MillionSecond	延时毫秒数
 * @note	由于协程不是基于时间片调度，这个函数只能延时最小时间，往往可能会比这个时间要长
 */
VOID
CoDelayExecutionAtLeast(
	PVOID Fiber,
	DWORD MillionSecond
) {

	//从TLS中获取协程实例
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	//申请一个延时执行对象
	PCOROUTINE_EXECUTE_DELAY DelayExecution = (PCOROUTINE_EXECUTE_DELAY)malloc(sizeof(COROUTINE_EXECUTE_DELAY));
	DelayExecution->Fiber = Fiber;
	DelayExecution->TimeAtLeast = GetTickCount64() + MillionSecond;

	//如果延时队列为空
	if (Instance->DelayExecutionList->empty()) {
		Instance->DelayExecutionList->push_front(DelayExecution);
		return;
	}

	//如果比最小延时还要小
	if (DelayExecution->TimeAtLeast < Instance->DelayExecutionList->front()->TimeAtLeast) {
		Instance->DelayExecutionList->push_front(DelayExecution);
		return;
	}

	//插入到对应的位置上
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

	SwitchToFiber(Instance->ScheduleRoutine);

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

	//创建线程相关的IO完成端口
	Instance->Iocp= CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (Instance->Iocp == NULL)
		goto ERROR_EXIT;

	//创建协程调度队列
	Instance->FiberList = new list<void*>;
	Instance->DelayExecutionList = new list<PCOROUTINE_EXECUTE_DELAY>;
	Instance->InitialRoutine = CreateFiber(StackSize, InitRoutine, Parameter);

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



