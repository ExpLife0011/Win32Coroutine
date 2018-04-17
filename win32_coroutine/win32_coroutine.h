#pragma once

#include <Windows.h>
#include <list>

#include "win32_hook.h"
#include "win32_sysroutine.h"

#define PERF_TEST

#define ASIO_FILE		1
#define ASIO_NET		2

//重叠对象扩展结构
typedef struct _COROUTINE_OVERLAPPED_WARPPER {
	DWORD Signature;
	OVERLAPPED Overlapped;
	DWORD BytesTransfered;
	DWORD ErrorCode;
	DWORD AsioType;
	PVOID Fiber;
	HANDLE Handle;
}COROUTINE_OVERLAPPED_WARPPER, *PCOROUTINE_OVERLAPPED_WARPPER;

//兼容线程格式的纤程调用
typedef struct _COROUTINE_COMPAT_CALL {
	LPTHREAD_START_ROUTINE CompatRoutine;
	LPVOID Parameter;
}COROUTINE_COMPAT_CALL, *PCOROUTINE_COMPAT_CALL;

//标准格式的纤程调用
typedef struct _COROUTINE_STANDARD_CALL {
	LPFIBER_START_ROUTINE FiberRoutine;
	LPVOID Parameter;
}COROUTINE_STANDARD_CALL, *PCOROUTINE_STANDARD_CALL;

//延时执行对象
typedef struct _COROUTINE_EXECUTE_DELAY {
	DWORD64 TimeAtLeast;
	LPVOID Fiber;
}COROUTINE_EXECUTE_DELAY, *PCOROUTINE_EXECUTE_DELAY;

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
CoYield(
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
 * 创建一个普通的纤程
 * 为了保证纤程对象能及时的回收，尽量调用这个接口
 */
BOOLEAN
CoInsertStandardRoutine(
	SIZE_T StackSize,
	LPFIBER_START_ROUTINE StartRoutine,
	LPVOID Parameter,
	PCOROUTINE_INSTANCE Instance
);

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
);

/**
 * 手动启动调度
 */
BOOLEAN
CoStartCoroutineManually(
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