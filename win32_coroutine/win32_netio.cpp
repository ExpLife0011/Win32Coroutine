

#include "win32_sysroutine.h"
#include "win32_coroutine.h"

#include <ws2tcpip.h>
#include <mswsock.h>

Routine_accept		System_accept;
Routine_recv		System_recv;

/**
 * �Զ����֧��Э�̵�accept
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

	//��ȡAccepteEx����
	Result = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&AcceptEx, sizeof(AcceptEx),
		&Bytes, NULL, NULL);
	if (Result == SOCKET_ERROR) {
		goto NATIVE_CALL;
	}

	//����һ������SOCKET
	AcceptSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (AcceptSocket == INVALID_SOCKET) {
		goto NATIVE_CALL;
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

	//����
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

	//����IOʧ��
	if (OverlappedWarpper->ErrorCode != ERROR_SUCCESS) {
		goto ERROR_EXIT;
	}

	//�µ�SOCKET�Ӿɵ�SOCKET�̳�����
	if (setsockopt(AcceptSocket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)s,
		sizeof(s)) == 0) {
		goto ERROR_EXIT;
	}

	//��ȡĿ�ĵ�ַ��Ϣ
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

	//����һ��Overlapped��������
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		WSASetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return 0;
	}
	
	//�첽����
	WsaBuf.buf = buf;
	WsaBuf.len = len;
	Result = WSARecv(s, &WsaBuf, 1, NULL, 0, &OverlappedWarpper->Overlapped, NULL);
	if ((Result !=SOCKET_ERROR)||(WSAGetLastError()!=WSA_IO_PENDING)) { 
		free(OverlappedWarpper);
		return 0;
	}

	CoSyncExecute(FALSE);

	//���ý��
	WSASetLastError(OverlappedWarpper->ErrorCode);
	Result = OverlappedWarpper->BytesTransfered;

	free(OverlappedWarpper);

	return Result;
}
