

#include "win32_sysroutine.h"
#include "win32_coroutine.h"

#include <ws2tcpip.h>
#include <mswsock.h>

Routine_accept		System_accept;

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

	//申请一个Overlapped的上下文
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		goto ERROR_EXIT_2;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));

	//申请接收缓存
	OverlappedWarpper->AcceptBuffer = malloc((sizeof(sockaddr_in) + 16) * 2);
	if (OverlappedWarpper->AcceptBuffer == NULL) {
		goto ERROR_EXIT_2;
	}

	//获取AccepteEx函数
	Result = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&AcceptEx, sizeof(AcceptEx),
		&Bytes, NULL, NULL);
	if (Result == SOCKET_ERROR) {
		goto ERROR_EXIT_2;
	}

	//创建一个接受SOCKET
	AcceptSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (AcceptSocket == INVALID_SOCKET) {
		goto ERROR_EXIT_2;
	}

	//设置不可继承
	if (!SetHandleInformation((HANDLE)AcceptSocket, HANDLE_FLAG_INHERIT, 0)) {
		goto ERROR_EXIT_2;
	}

	//接收
	Result = AcceptEx(s,
		AcceptSocket, 
		OverlappedWarpper,
		0,
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&Bytes,
		&OverlappedWarpper->Overlapped);
	if (Result == FALSE) {
		goto ERROR_EXIT_2;
	}

	CoSyncExecute(FALSE);

	//接受IO失败
	if (OverlappedWarpper->ErrorCode != ERROR_SUCCESS) {
		goto ERROR_EXIT_1;
	}

	//新的SOCKET从旧的SOCKET继承属性
	if (setsockopt(AcceptSocket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)s,
		sizeof(s)) == 0) {
		goto ERROR_EXIT_1;
	}

	//获取目的地址信息
	getpeername(AcceptSocket, addr, addrlen);

	free(OverlappedWarpper);
	return AcceptSocket;

ERROR_EXIT_1:
	free(OverlappedWarpper);

	closesocket(AcceptSocket);

	return INVALID_SOCKET;

ERROR_EXIT_2:
	if (OverlappedWarpper != NULL) {
		free(OverlappedWarpper);
	}

	if (AcceptSocket) {
		closesocket(AcceptSocket);
	}

	goto NATIVE_CALL;
}