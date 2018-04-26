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

//------------------------------------//
//-------------netio函数--------------//
//------------------------------------//

typedef
SOCKET
(WSAAPI* Routine_socket)(
	_In_ int af,
	_In_ int type,
	_In_ int protocol
);

typedef
SOCKET 
(WSAAPI* Routine_accept)(
	_In_ SOCKET s,
	_Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
	_Inout_opt_ int FAR *addrlen
	);

typedef
int 
(WSAAPI* Routine_recv)(
	_In_ SOCKET s,
	_Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
	_In_ int len,
	_In_ int flags
	);

typedef
int
(WSAAPI* Routine_recvfrom)(
	_In_ SOCKET s,
	_Out_writes_bytes_to_(len, return) __out_data_source(NETWORK) char FAR * buf,
	_In_ int len,
	_In_ int flags,
	_Out_writes_bytes_to_opt_(*fromlen, *fromlen) struct sockaddr FAR * from,
	_Inout_opt_ int FAR * fromlen
	);

typedef
int 
(WSAAPI* Routine_send)(
	_In_ SOCKET s,
	_In_reads_bytes_(len) const char FAR * buf,
	_In_ int len,
	_In_ int flags
	);

typedef
int 
(WSAAPI* Routine_sendto)(
	_In_ SOCKET s,
	_In_reads_bytes_(len) const char FAR * buf,
	_In_ int len,
	_In_ int flags,
	_In_reads_bytes_opt_(tolen) const struct sockaddr FAR *to,
	_In_ int tolen
	);

namespace Win32Coroutine {

	namespace FileIo {

		extern Routine_CreateFileW		System_CreateFileW;
		extern Routine_ReadFile			System_ReadFile;
		extern Routine_WriteFile		System_WriteFile;
		extern Routine_DeviceIoControl	System_DeviceIoControl;

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

	}

	namespace NetIo {

		extern Routine_accept		System_accept;
		extern Routine_recv			System_recv;
		extern Routine_send			System_send;
		extern Routine_socket		System_socket;

		/**
		 * 自定义的支持协程的socket
		 */
		SOCKET
			WSAAPI
			Coroutine_socket(
				_In_ int af,
				_In_ int type,
				_In_ int protocol
			);

		/**
		 * 自定义的支持协程的accept
		 */
		SOCKET
			WSAAPI
			Coroutine_accept(
				_In_ SOCKET s,
				_Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
				_Inout_opt_ int FAR *addrlen
			);

		/**
		 * 自定义的支持协程的recv
		 */
		int
			WSAAPI
			Coroutine_recv(
				_In_ SOCKET s,
				_Out_writes_bytes_to_(len, return) char FAR * buf,
				_In_ int len,
				_In_ int flags
			);

		/**
		 * 自定义的支持协程的send
		 */
		int
			WSAAPI
			Coroutine_send(
				_In_ SOCKET s,
				_In_reads_bytes_(len) const char FAR * buf,
				_In_ int len,
				_In_ int flags
			);

	}

}