#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

#define PERF_TEST

//重叠对象扩展结构
typedef struct _COROUTINE_OVERLAPPED_WARPPER {
	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	DWORD ErrorCode;
	PVOID Fiber;
	PVOID AcceptBuffer;
}COROUTINE_OVERLAPPED_WARPPER, *PCOROUTINE_OVERLAPPED_WARPPER;

//兼容线程格式的纤程调用
typedef struct _COROUTINE_COMPAT_CALL {
	LPTHREAD_START_ROUTINE CompatRoutine;
	LPVOID Parameter;
}COROUTINE_COMPAT_CALL, *PCOROUTINE_COMPAT_CALL;

//延时执行对象
typedef struct _COROUTINE_EXECUTE_DELAY {
	DWORD64 TimeAtLeast;
	LPVOID Fiber;
}COROUTINE_EXECUTE_DELAY, *PCOROUTINE_EXECUTE_DELAY;

//消息队列对象
typedef struct _COROUTINE_MESSAGE_QUEUE {
	SLIST_HEADER QueueHeader;
	PVOID Fiber;
}COROUTINE_MESSAGE_QUEUE, *PCOROUTINE_MESSAGE_QUEUE;

//消息队列节点
typedef struct _COROUTINE_MESSAGE_NODE {
	SLIST_ENTRY QueueNode;
	PVOID UserBuffer;
	SIZE_T BufferSize;
}COROUTINE_MESSAGE_NODE, *PCOROUTINE_MESSAGE_NODE;

//纤程上下文
typedef struct _COROUTINE_FIBER_CONTEXT {
	LPVOID UserParameter;
	COROUTINE_MESSAGE_QUEUE InternalMessageQueue;
}COROUTINE_FIBER_CONTEXT;

//一个协程实例
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
 * 手动进行协程调度
 */
VOID
CoSyncExecute(
	BOOLEAN Terminate
);

/**
 * 创建一个协程
 */
BOOLEAN
CoInsertRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
);

/**
 * 创建一个兼容线程ABI的协程
 */
BOOLEAN
CoInsertCompatRoutine(
	SIZE_T StackSize,
	LPTHREAD_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
);

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
);