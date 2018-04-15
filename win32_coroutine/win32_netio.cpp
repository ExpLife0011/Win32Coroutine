

#include "win32_sysroutine.h"
#include "win32_coroutine.h"

#include <ws2tcpip.h>
#include <mswsock.h>

Routine_accept		System_accept;

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

	//����һ��Overlapped��������
	PCOROUTINE_OVERLAPPED_WARPPER OverlappedWarpper = (PCOROUTINE_OVERLAPPED_WARPPER)malloc(sizeof(COROUTINE_OVERLAPPED_WARPPER));
	if (OverlappedWarpper == NULL) {
		goto ERROR_EXIT_2;
	}
	memset(OverlappedWarpper, 0, sizeof(COROUTINE_OVERLAPPED_WARPPER));

	//������ջ���
	OverlappedWarpper->AcceptBuffer = malloc((sizeof(sockaddr_in) + 16) * 2);
	if (OverlappedWarpper->AcceptBuffer == NULL) {
		goto ERROR_EXIT_2;
	}

	//��ȡAccepteEx����
	Result = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&AcceptEx, sizeof(AcceptEx),
		&Bytes, NULL, NULL);
	if (Result == SOCKET_ERROR) {
		goto ERROR_EXIT_2;
	}

	//����һ������SOCKET
	AcceptSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (AcceptSocket == INVALID_SOCKET) {
		goto ERROR_EXIT_2;
	}

	//���ò��ɼ̳�
	if (!SetHandleInformation((HANDLE)AcceptSocket, HANDLE_FLAG_INHERIT, 0)) {
		goto ERROR_EXIT_2;
	}

	//����
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

	//����IOʧ��
	if (OverlappedWarpper->ErrorCode != ERROR_SUCCESS) {
		goto ERROR_EXIT_1;
	}

	//�µ�SOCKET�Ӿɵ�SOCKET�̳�����
	if (setsockopt(AcceptSocket,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)s,
		sizeof(s)) == 0) {
		goto ERROR_EXIT_1;
	}

	//��ȡĿ�ĵ�ַ��Ϣ
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