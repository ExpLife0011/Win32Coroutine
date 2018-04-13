
#include "win32_sysroutine.h"
#include "win32_coroutine.h"

Routine_CreateFileW System_CreateFileW;
Routine_ReadFile System_ReadFile;

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

	if (!IsThreadAFiber()) {
		return System_ReadFile(hFile,
			lpBuffer,
			nNumberOfBytesToRead,
			lpNumberOfBytesRead,
			lpOverlapped
		);
	}

	BOOL Succeed = TRUE;
	LARGE_INTEGER OriginalOffset, ZeroOffset;
	PASYNC_CONTEXT AsyncContext = (PASYNC_CONTEXT)malloc(sizeof(ASYNC_CONTEXT));
	if (AsyncContext == NULL) {
		*lpNumberOfBytesRead = 0;
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return FALSE;
	}
	memset(AsyncContext, 0, sizeof(ASYNC_CONTEXT));

	ZeroOffset.QuadPart = 0;

	SetFilePointerEx(hFile, ZeroOffset, &OriginalOffset, FILE_CURRENT);
	AsyncContext->Overlapped.Offset = OriginalOffset.LowPart;
	AsyncContext->Overlapped.OffsetHigh = OriginalOffset.HighPart;
	AsyncContext->Fiber = GetCurrentFiber();

	Succeed = System_ReadFile(hFile,
		lpBuffer,
		nNumberOfBytesToRead,
		lpNumberOfBytesRead,
		&AsyncContext->Overlapped
	);
	if (Succeed || GetLastError() != ERROR_IO_PENDING) {
		goto EXIT;
	}

	CoSyncExecute();
	if (AsyncContext->BytesTransfered == 0) {
		goto EXIT;
	}

	Succeed = TRUE;

EXIT:
	*lpNumberOfBytesRead = AsyncContext->BytesTransfered;
	free(AsyncContext);

	return Succeed;
}