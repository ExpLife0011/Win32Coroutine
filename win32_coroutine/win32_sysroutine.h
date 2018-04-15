#pragma once 

#include <WinSock2.h>
#include <Windows.h>


//------------------------------------//
//------------fileio函数--------------//
//------------------------------------//

typedef
HANDLE
(WINAPI* Routine_CreateFileW)(
	_In_ LPCWSTR lpFileName,
	_In_ DWORD dwDesiredAccess,
	_In_ DWORD dwShareMode,
	_In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	_In_ DWORD dwCreationDisposition,
	_In_ DWORD dwFlagsAndAttributes,
	_In_opt_ HANDLE hTemplateFile
	);

typedef
BOOL
(WINAPI* Routine_ReadFile)(
	_In_ HANDLE hFile,
	_Out_writes_bytes_to_opt_(nNumberOfBytesToRead, *lpNumberOfBytesRead) __out_data_source(FILE) LPVOID lpBuffer,
	_In_ DWORD nNumberOfBytesToRead,
	_Out_opt_ LPDWORD lpNumberOfBytesRead,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
	);

typedef
BOOL
(WINAPI* Routine_WriteFile)(
	_In_ HANDLE hFile,
	_In_reads_bytes_opt_(nNumberOfBytesToWrite) LPCVOID lpBuffer,
	_In_ DWORD nNumberOfBytesToWrite,
	_Out_opt_ LPDWORD lpNumberOfBytesWritten,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
	);

typedef
BOOL
(WINAPI* Routine_DeviceIoControl)(
	_In_ HANDLE hDevice,
	_In_ DWORD dwIoControlCode,
	_In_reads_bytes_opt_(nInBufferSize) LPVOID lpInBuffer,
	_In_ DWORD nInBufferSize,
	_Out_writes_bytes_to_opt_(nOutBufferSize, *lpBytesReturned) LPVOID lpOutBuffer,
	_In_ DWORD nOutBufferSize,
	_Out_opt_ LPDWORD lpBytesReturned,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
);

extern Routine_CreateFileW System_CreateFileW;
extern Routine_ReadFile System_ReadFile;
extern Routine_WriteFile System_WriteFile;
extern Routine_DeviceIoControl System_DeviceIoControl;

//------------------------------------//
//-------------netio函数--------------//
//------------------------------------//

typedef
SOCKET 
(PASCAL FAR* Routine_accept)(
	_In_ SOCKET s,
	_Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
	_Inout_opt_ int FAR *addrlen
	);

typedef
int 
(PASCAL* Routine_recv)(
	_In_ SOCKET s,
	_Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
	_In_ int len,
	_In_ int flags
	);

typedef
int
(PASCAL FAR* Routine_recvfrom)(
	_In_ SOCKET s,
	_Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
	_In_ int len,
	_In_ int flags,
	_Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR * from,
	_Inout_opt_ int FAR * fromlen
	);

typedef
int 
(PASCAL FAR* Routine_send)(
	_In_ SOCKET s,
	_In_reads_bytes_(len) const char FAR * buf,
	_In_ int len,
	_In_ int flags
	);

typedef
int 
(PASCAL FAR* Routine_sendto)(
	_In_ SOCKET s,
	_In_reads_bytes_(len) const char FAR * buf,
	_In_ int len,
	_In_ int flags,
	_In_reads_bytes_opt_(tolen) const struct sockaddr FAR *to,
	_In_ int tolen
	);

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
);

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
);

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
);

/**
 * 自定义的支持协程的DeviceIoControl
 */
BOOL
WINAPI
Coroutine_DeviceIoControl(
	_In_ HANDLE hDevice,
	_In_ DWORD dwIoControlCode,
	_In_reads_bytes_opt_(nInBufferSize) LPVOID lpInBuffer,
	_In_ DWORD nInBufferSize,
	_Out_writes_bytes_to_opt_(nOutBufferSize, *lpBytesReturned) LPVOID lpOutBuffer,
	_In_ DWORD nOutBufferSize,
	_Out_opt_ LPDWORD lpBytesReturned,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
);

/**
 * 自定义的支持协程的accept
 */
SOCKET
PASCAL FAR
Coroutine_accept(
	_In_ SOCKET s,
	_Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
	_Inout_opt_ int FAR *addrlen
);