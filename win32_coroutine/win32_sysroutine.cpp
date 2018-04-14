
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

Routine_CreateFileW		System_CreateFileW;
Routine_ReadFile		System_ReadFile;
Routine_WriteFile System_WriteFile;

/**
 * 自定义的支持协程的CreateFileW
 */
HANDLE
WINAPI
Coroutine_CreateFileW(
	_In_ LPCWSTR lpFileName,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
) {
	
	if (!IsThreadAFiber()) {
		return System_CreateFileW(lpFileName,
			dwDesiredAccess,
			dwShareMode,
			lpSecurityAttributes,
			dwCreationDisposition,
			dwFlagsAndAttributes,
			hTemplateFile
		);
	}

	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);
	HANDLE FileHandle = System_CreateFileW(lpFileName,
		dwDesiredAccess,
		dwShareMode,
		lpSecurityAttributes,
		dwCreationDisposition,
		dwFlagsAndAttributes | FILE_FLAG_OVERLAPPED,
		hTemplateFile
	);
	DWORD err = GetLastError();
	if (FileHandle == INVALID_HANDLE_VALUE)
		return FileHandle;

	CreateIoCompletionPort(FileHandle, Instance->Iocp, (ULONG_PTR)NULL, 0);

	return (HANDLE)FileHandle;
}

/**
 * 自定义的支持协程的ReadFile
 */
BOOL
WINAPI
Coroutine_ReadFile(
	_In_ HANDLE hFile,
	_Out_writes_bytes_to_opt_(nNumberOfBytesToRead, *lpNumberOfBytesRead) __out_data_source(FILE) LPVOID lpBuffer,
	_In_ DWORD nNumberOfBytesToRead,
	_Out_opt_ LPDWORD lpNumberOfBytesRead,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
) {

	//判断是不是纤程
	if (!IsThreadAFiber()) {
		return System_ReadFile(hFile,
			lpBuffer,
			nNumberOfBytesToRead,
			lpNumberOfBytesRead,
			lpOverlapped
		);
	}

	BOOL Succeed = TRUE, Restore;
	LARGE_INTEGER OriginalOffset, ZeroOffset;

	//申请一个Overlapped的上下文
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		*lpNumberOfBytesRead = 0;
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));

	ZeroOffset.QuadPart = 0;

	if (SetFilePointerEx(hFile, ZeroOffset, &OriginalOffset, FILE_CURRENT)) {
		Restore = TRUE;
	}

	OverlappedWarpper->Overlapped.Offset = OriginalOffset.LowPart;
	OverlappedWarpper->Overlapped.OffsetHigh = OriginalOffset.HighPart;
	OverlappedWarpper->Fiber = GetCurrentFiber();

	if (Restore) {
		SetFilePointerEx(hFile, OriginalOffset, &OriginalOffset, FILE_BEGIN);
	}

	Succeed = System_ReadFile(hFile,
		lpBuffer,
		nNumberOfBytesToRead,
		lpNumberOfBytesRead,
		&OverlappedWarpper->Overlapped
	);
	if (Succeed || GetLastError() != ERROR_IO_PENDING) {
		goto EXIT;
	}

	//手动调度纤程
	CoSyncExecute(FALSE);
	
	if (OverlappedWarpper->BytesTransfered == 0) {
		goto EXIT;
	}

	Succeed = TRUE;

EXIT:
	*lpNumberOfBytesRead = OverlappedWarpper->BytesTransfered;
	free(OverlappedWarpper);

	return Succeed;
}

/**
 * 自定义的支持协程的WriteFile
 */
BOOL
WINAPI
Coroutine_WriteFile(
	_In_ HANDLE hFile,
	_In_reads_bytes_opt_(nNumberOfBytesToWrite) LPCVOID lpBuffer,
	_In_ DWORD nNumberOfBytesToWrite,
	_Out_opt_ LPDWORD lpNumberOfBytesWritten,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
) {

	//判断是不是纤程
	if (!IsThreadAFiber()) {
		return System_WriteFile(hFile,
			lpBuffer,
			nNumberOfBytesToWrite,
			lpNumberOfBytesWritten,
			lpOverlapped
		);
	}

	BOOL Succeed = TRUE, Restore;
	LARGE_INTEGER OriginalOffset = { 0 }, ZeroOffset;

	//申请一个Overlapped的上下文
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		*lpNumberOfBytesWritten = 0;
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}
	memset(OverlappedWarpper, 0, sizeof(OverlappedWarpper));

	ZeroOffset.QuadPart = 0;

	if (SetFilePointerEx(hFile, ZeroOffset, &OriginalOffset, FILE_CURRENT)) {
		Restore = TRUE;
	}
	
	OverlappedWarpper->Overlapped.Offset = OriginalOffset.LowPart;
	OverlappedWarpper->Overlapped.OffsetHigh = OriginalOffset.HighPart;
	OverlappedWarpper->Fiber = GetCurrentFiber();

	if (Restore) {
		SetFilePointerEx(hFile, OriginalOffset, &OriginalOffset, FILE_BEGIN);
	}

	Succeed = System_WriteFile(hFile,
		lpBuffer,
		nNumberOfBytesToWrite,
		lpNumberOfBytesWritten,
		&OverlappedWarpper->Overlapped
	);
	if (Succeed || GetLastError() != ERROR_IO_PENDING) {
		goto EXIT;
	}

	//手动调度纤程
	CoSyncExecute(FALSE);

	if (OverlappedWarpper->BytesTransfered == 0) {
		goto EXIT;
	}

	Succeed = TRUE;

EXIT:
	*lpNumberOfBytesWritten = OverlappedWarpper->BytesTransfered;
	free(OverlappedWarpper);

	return Succeed;
}