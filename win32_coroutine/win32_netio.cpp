

#include "win32_sysroutine.h"
#include "win32_coroutine.h"

#include <ws2tcpip.h>
#include <mswsock.h>

Routine_accept		System_accept;
Routine_recv		System_recv;

/**
 * 自定义的支持协程的accept
 */
SOCKET 
PASCAL FAR
Coroutine_accept(
	_In_ SOCKET s,
	_Out_writes_bytes_opt_(*addrlen) struct sockaddr FAR *addr,
	_Inout_opt_ int FAR *addrlen
) {

	if (!IsThreadAFiber()) {
NATIVE_CALL:
		return System_accept(s, addr, addrlen);
	}

	int Result;
	SOCKET AcceptSocket = 0;
	LPFN_ACCEPTEX AcceptEx;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD Bytes;
	char AcceptBuffer[(sizeof(sockaddr_in) + 16) * 2];

	//获取AccepteEx函数
	Result = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&AcceptEx, sizeof(AcceptEx),
		&Bytes, NULL, NULL);
	if (Result == SOCKET_ERROR) {
		goto NATIVE_CALL;
	}

	//创建一个接受SOCKET
	AcceptSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (AcceptSocket == INVALID_SOCKET) {
		goto NATIVE_CALL;
	}

	//设置不可继承
	if (!SetHandleInformation((HANDLE)AcceptSocket, HANDLE_FLAG_INHERIT, 0)) {
		closesocket(AcceptSocket);
		goto NATIVE_CALL;
	}

	//申请一个Overlapped的上下文
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		closesocket(AcceptSocket);
		goto NATIVE_CALL;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));

	//接收
	Result = AcceptEx(s,
		AcceptSocket, 
		AcceptBuffer,
		0,
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&Bytes,
		&OverlappedWarpper->Overlapped);
	if (Result == FALSE) {
		goto ERROR_EXIT;
	}

	CoSyncExecute(FALSE);

	//接受IO失败
	if (OverlappedWarpper->ErrorCode != ERROR_SUCCESS) {
		goto ERROR_EXIT;
	}

	//新的SOCKET从旧的SOCKET继承属性
	if (setsockopt(AcceptSocket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)s,
		sizeof(s)) == 0) {
		goto ERROR_EXIT;
	}

	//获取目的地址信息
	getpeername(AcceptSocket, addr, addrlen);

	free(OverlappedWarpper);
	return AcceptSocket;

ERROR_EXIT:
	free(OverlappedWarpper);

	closesocket(AcceptSocket);

	return INVALID_SOCKET;
}

int
PASCAL
Coroutine_recv(
	_In_ SOCKET s,
	_Out_writes_bytes_to_(len, return) char FAR * buf,
	_In_ int len,
	_In_ int flags
) {

	if (!IsThreadAFiber()) {
		return System_recv(s, buf, len, flags);
	}

	int Result;
	WSABUF WsaBuf;

	//申请一个Overlapped的上下文
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		WSASetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return 0;
	}
	
	//异步接受
	WsaBuf.buf = buf;
	WsaBuf.len = len;
	Result = WSARecv(s, &WsaBuf, 1, NULL, 0, &OverlappedWarpper->Overlapped, NULL);
	if ((Result !=SOCKET_ERROR)||(WSAGetLastError()!=WSA_IO_PENDING)) { 
		free(OverlappedWarpper);
		return 0;
	}

	CoSyncExecute(FALSE);

	//设置结果
	WSASetLastError(OverlappedWarpper->ErrorCode);
	Result = OverlappedWarpper->BytesTransfered;

	free(OverlappedWarpper);

	return Result;
}
