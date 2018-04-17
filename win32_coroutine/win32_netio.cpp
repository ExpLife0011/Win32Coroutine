

#include "win32_sysroutine.h"
#include "win32_coroutine.h"

#include <ws2tcpip.h>
#include <mswsock.h>

Routine_accept		System_accept;
Routine_recv		System_recv;
Routine_send		System_send;
Routine_socket		System_socket;

/**
 * �Զ����֧��Э�̵�socket
 */
SOCKET
WSAAPI
Coroutine_socket(
	_In_ int af,
	_In_ int type,
	_In_ int protocol
) {

	if (!IsThreadAFiber()) {
		return System_socket(af, type, protocol);
	}

	DWORD Yes = 1;
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	//����һ��socket
	SOCKET s = System_socket(af, type, protocol);
	if (s == INVALID_SOCKET)
		return s;

	//����SOCKETΪnbio
	if (ioctlsocket(s, FIONBIO, &Yes) == SOCKET_ERROR) {
		goto ERROR_EXIT;
	}

	//���þ��Ϊ���ɼ̳�
	if (!SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0)) {
		goto ERROR_EXIT;
	}

	//��socket�󶨵�IO�˿�
	if (CreateIoCompletionPort((HANDLE)s, Instance->Iocp, (ULONG_PTR)s, 1) == NULL) {
		goto ERROR_EXIT;
	}

	return s;

ERROR_EXIT:
	closesocket(s);
	return INVALID_SOCKET;
}

/**
 * �Զ����֧��Э�̵�accept
 */
SOCKET 
WSAAPI
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
	static LPFN_ACCEPTEX AcceptEx = NULL;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	DWORD Bytes, Yes = 1;
	char AcceptBuffer[(sizeof(sockaddr_in) + 16) * 2];
	PCOROUTINE_INSTANCE Instance = (PCOROUTINE_INSTANCE)TlsGetValue(0);

	if (AcceptEx == NULL) {

		//��ȡAccepteEx����
		Result = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx, sizeof(GuidAcceptEx),
			&AcceptEx, sizeof(AcceptEx),
			&Bytes, NULL, NULL);
		if (Result == SOCKET_ERROR) {
			goto NATIVE_CALL;
		}
	}

	//����һ������SOCKET
	AcceptSocket = System_socket(AF_INET, SOCK_STREAM, 0);
	if (AcceptSocket == INVALID_SOCKET) {
		goto NATIVE_CALL;
	}

	//����SOCKETΪnbio
	if (ioctlsocket(s, FIONBIO, &Yes) == SOCKET_ERROR) {
		closesocket(AcceptSocket);
		goto ERROR_EXIT;
	}

	//���ò��ɼ̳�
	if (!SetHandleInformation((HANDLE)AcceptSocket, HANDLE_FLAG_INHERIT, 0)) {
		closesocket(AcceptSocket);
		goto NATIVE_CALL;
	}

	//����һ��Overlapped��������
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		closesocket(AcceptSocket);
		goto NATIVE_CALL;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));

	OverlappedWarpper->Fiber = GetCurrentFiber();
	OverlappedWarpper->AsioType = ASIO_NET;
	OverlappedWarpper->Handle = (HANDLE)s;

	//����
	Result = AcceptEx(s,
		AcceptSocket,
		AcceptBuffer,
		0,
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&Bytes,
		&OverlappedWarpper->Overlapped);
	if (Result == FALSE&&WSAGetLastError() != WSA_IO_PENDING) {
		goto ERROR_EXIT;
	}

	CoYield(FALSE);

	//����IOʧ��
	if (OverlappedWarpper->ErrorCode != ERROR_SUCCESS) {
		goto ERROR_EXIT;
	}

	//�µ�SOCKET�Ӿɵ�SOCKET�̳�����
	if (setsockopt(AcceptSocket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)&s,
		sizeof(s)) != 0) {
		goto ERROR_EXIT;
	}

	//��ȡĿ�ĵ�ַ��Ϣ
	getpeername(AcceptSocket, addr, addrlen);

	//�󶨵�Э�̵�IOCP��
	if (CreateIoCompletionPort((HANDLE)AcceptSocket, Instance->Iocp, (ULONG_PTR)AcceptSocket, 1) == NULL) {
		goto ERROR_EXIT;
	}

	//��������IOCP֪ͨ
	SetFileCompletionNotificationModes((HANDLE)AcceptSocket,
		FILE_SKIP_SET_EVENT_ON_HANDLE |
		FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

	free(OverlappedWarpper);
	return AcceptSocket;

ERROR_EXIT:
	free(OverlappedWarpper);

	closesocket(AcceptSocket);

	return INVALID_SOCKET;
}

/**
 * �Զ����֧��Э�̵�recv
 */
int
WSAAPI
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
	DWORD Bytes, Flags = 0;

	//����һ��Overlapped��������
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		WSASetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return 0;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));
	
	OverlappedWarpper->Fiber = GetCurrentFiber();
	OverlappedWarpper->AsioType = ASIO_NET;

	//�첽����
	WsaBuf.buf = buf;
	WsaBuf.len = len;
	Result = WSARecv(s, &WsaBuf, 1, &Bytes, &Flags, &OverlappedWarpper->Overlapped, NULL);
	if ((Result != SOCKET_ERROR) || (WSAGetLastError() != WSA_IO_PENDING)) {
		free(OverlappedWarpper);
		return Bytes;
	}

	CoYield(FALSE);

	//���ý��
	WSASetLastError(OverlappedWarpper->ErrorCode);
	Result = OverlappedWarpper->BytesTransfered;

	free(OverlappedWarpper);

	return Result;
}

/**
 * �Զ����֧��Э�̵�send
 */
int 
WSAAPI
Coroutine_send(
	_In_ SOCKET s,
	_In_reads_bytes_(len) const char FAR * buf,
	_In_ int len,
	_In_ int flags
) {

	if (!IsThreadAFiber()) {
		return System_send(s, buf, len, flags);
	}

	int Result;
	WSABUF WsaBuf;
	DWORD Bytes, Flags = 0;

	//����һ��Overlapped��������
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		WSASetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return 0;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));

	OverlappedWarpper->Fiber = GetCurrentFiber();
	OverlappedWarpper->AsioType = ASIO_NET;

	//�첽����
	WsaBuf.buf = (char*)buf;
	WsaBuf.len = len;
	Result = WSASend(s, &WsaBuf, 1, &Bytes, Flags, &OverlappedWarpper->Overlapped, NULL);
	if ((Result != SOCKET_ERROR) || (WSAGetLastError() != WSA_IO_PENDING)) {
		free(OverlappedWarpper);
		return Bytes;
	}

	CoYield(FALSE);

	//���ý��
	WSASetLastError(OverlappedWarpper->ErrorCode);
	Result = OverlappedWarpper->BytesTransfered;

	free(OverlappedWarpper);

	return Result;
}