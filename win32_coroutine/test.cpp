#pragma once

#include "stdafx.h"
#include "win32_coroutine.h"
#include <compressapi.h>


//#pragma comment(lib,"Cabinet.lib")
#pragma comment(lib,"ws2_32.lib")

//#define TEST
#define THREAD

DWORD s;
DWORD end_time = 0;

HANDLE Event;

int COUNT = 5000;

VOID
WINAPI CoRoutine1(
	LPVOID lpFiberParameter
) {

	CHAR buf[4096];
	DWORD Size;
	LONG Offset = 0;
	SIZE_T CSize;

	PVOID CompressData;
	
	HANDLE g = CreateFile(L"G:\\1.DMP", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	for (int i = 0;i < COUNT;i++) {
		SetFilePointer(g, 4096 * i, &Offset, FILE_BEGIN);
		ReadFile(g, buf, 4096, &Size, NULL);

		//SetFilePointer(g, 1 * 1024 * 1024 * 1024 - 4096 * i, &Offset, FILE_BEGIN);
		//ReadFile(g, buf, 4096, &Size, NULL);

#ifdef TEST
		COMPRESSOR_HANDLE Com = NULL;
		CompressData = malloc(4096);
		CreateCompressor(COMPRESS_ALGORITHM_LZMS, NULL, &Com);
		if (Com != NULL) {
			Compress(Com, buf, 4096, CompressData, 4096, &CSize);
			CloseCompressor(Com);
		}
		free(CompressData);
#endif

	}

	if (end_time < GetTickCount()) {
		end_time = GetTickCount();
#ifdef THREAD
		SetEvent(Event);
#endif
	}

	CloseHandle(g);
}

VOID
WINAPI CoRoutine2(
	LPVOID lpFiberParameter
) {

	CHAR buf[4096];
	DWORD Size;
	LONG Offset = 0;
	SIZE_T CSize;

	PVOID CompressData;

	HANDLE g = CreateFile(L"G:\\1.DMP", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);

	for (int i = 0;i < COUNT;i++) {
		SetFilePointer(g, 4096 * i, &Offset, FILE_BEGIN);
		ReadFile(g, buf, 4096, &Size, NULL);

		//SetFilePointer(g, 1 * 1024 * 1024 * 1024 - 4096 * i, &Offset, FILE_BEGIN);
		//ReadFile(g, buf, 4096, &Size, NULL);

#ifdef TEST
		COMPRESSOR_HANDLE Com = NULL;
		CompressData = malloc(4096);
		CreateCompressor(COMPRESS_ALGORITHM_LZMS, NULL, &Com);
		if (Com != NULL) {
			Compress(Com, buf, 4096, CompressData, 4096, &CSize);
			CloseCompressor(Com);
		}
		free(CompressData);
#endif

	}
	
	if (end_time < GetTickCount()) {
		end_time = GetTickCount();
#ifdef THREAD
		SetEvent(Event);
#endif
	}

	CloseHandle(g);
}

VOID
WINAPI LowStackMemroy(
	LPVOID lpFiberParameter
) {

	CHAR t[50000];

	t[49099] = 'c';
}

#define  PORT 6000  
DWORD WINAPI clientProc(LPARAM lparam)  
{     
    SOCKET sockClient = (SOCKET)lparam;  
    char buf[1024];  
    while (TRUE)  
    {  
        memset(buf, 0, sizeof(buf));  
        // ���տͻ��˵�һ������   
        int ret = recv(sockClient, buf, sizeof(buf), 0);  
        //����Ƿ����ʧ��  
        if (SOCKET_ERROR == ret)   
        {  
            printf("socket recv failed\n");  
            closesocket(sockClient);  
            return -1;  
        }  
        // 0 ����ͻ��������Ͽ�����  
        if (ret == 0)   
        {  
            printf("client close connection\n");  
            closesocket(sockClient);  
            return -1;  
        }         
        // ��������  
        ret = send(sockClient, buf, strlen(buf), 0);  
        //����Ƿ���ʧ��  
        if (SOCKET_ERROR == ret)  
        {  
            printf("socket send failed\n");  
            closesocket(sockClient);  
            return -1;  
        }  
    }  
    closesocket(sockClient);  
    return 0;  
}  

bool InitNetEnv()  
{  
    // �������绷���ĳ�ʼ������  
    WSADATA wsa;  
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)   
    {  
        printf("WSAStartup failed\n");  
        return false;  
    }  
    return true;  
}  

VOID
WINAPI
NetServerTest(
	LPVOID lpFiberParameter
) {

	if (!InitNetEnv())
	{
		return;
	}
	// ��ʼ����ɣ�����һ��TCP��socket  
	SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//����Ƿ񴴽�ʧ��  
	if (sServer == INVALID_SOCKET)
	{
		printf("socket failed\n");
		return;
	}
	printf("Create socket OK\n");
	//���а󶨲���  
	SOCKADDR_IN addrServ;
	addrServ.sin_family = AF_INET; // Э���ΪIPV4��  
	addrServ.sin_port = htons(PORT); // �˿�  ��Ϊ������С��ģʽ�������Ǵ��ģʽ������htons�ѱ����ֽ���תΪ�����ֽ���  
	addrServ.sin_addr.S_un.S_addr = INADDR_ANY; // ip��ַ��INADDR_ANY��ʾ�󶨵�������������IP  
												//��ɰ󶨲���  
	int ret = bind(sServer, (sockaddr *)&addrServ, sizeof(sockaddr));
	//�����Ƿ�ɹ�  
	if (SOCKET_ERROR == ret)
	{
		printf("socket bind failed\n");
		WSACleanup(); // �ͷ����绷��  
		closesocket(sServer); // �ر���������  
		return;
	}
	printf("socket bind OK\n");
	// �󶨳ɹ������м���  
	ret = listen(sServer, 10);
	//����Ƿ�����ɹ�  
	if (SOCKET_ERROR == ret)
	{
		printf("socket listen failed\n");
		WSACleanup();
		closesocket(sServer);
		return;
	}
	printf("socket listen OK\n");
	// �����ɹ�  
	sockaddr_in addrClient; // ���ڱ���ͻ��˵�����ڵ����Ϣ  
	int addrClientLen = sizeof(sockaddr_in);
	while (TRUE)
	{
		//�½�һ��socket�����ڿͻ���  
		SOCKET *sClient = new SOCKET;
		//�ȴ��ͻ��˵�����  
		*sClient = accept(sServer, (sockaddr*)&addrClient, &addrClientLen);
		if (INVALID_SOCKET == *sClient)
		{
			printf("socket accept failed\n");
			WSACleanup();
			closesocket(sServer);
			delete sClient;
			return;
		}
		//�����߳�Ϊ�ͻ����������շ�  
		//CreateThread(0, 0, (LPTHREAD_START_ROUTINE)clientProc, (LPVOID)*sClient, 0, 0);

		CoInsertCompatRoutine(0x1000, (LPTHREAD_START_ROUTINE)clientProc, (LPVOID)*sClient, NULL);
	}
	closesocket(sServer);
	WSACleanup();
	return;

}

VOID
WINAPI
InitRoutine(
	LPVOID lParameter
) {

	s = GetTickCount();
	//test
	//for (int i = 0;i < 10000;i++) {
	//	CoInsertRoutine(0x1000, CoRoutine1, NULL, NULL);
	//}
	//for (int i = 0;i < 10000;i++) {
	//	CoInsertRoutine(0x1000, CoRoutine2, NULL, NULL);
	//}
	CoInsertStandardRoutine(0x1000, NetServerTest, NULL, NULL);

	CoYield(TRUE);
}

int main()
{

	Event = CreateEvent(NULL, FALSE, FALSE, NULL);

#ifdef THREAD
	CoSetupWin32ApiHook(NULL);
	HANDLE CoHandle = CoCreateCoroutine(0x1000, InitRoutine, NULL, TRUE);

	WaitForSingleObject(Event, INFINITE);
	printf("%d\n", end_time - s);

#else

	for (int i = 1;i < 6;i++) {
		COUNT = i * 1000;
		s = GetTickCount();
		CoRoutine1(NULL);
		CoRoutine2(NULL);
		printf("%d\n", end_time - s);
	}

#endif

	

	getchar();
	return 0;
}