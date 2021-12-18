#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")
#define BUFSIZE 1024

typedef struct
{
    SOCKET hClntSock;
    SOCKADDR_IN clntAddr;
} PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

typedef struct
{
    OVERLAPPED overlapped;
    char buffer[BUFSIZE];
    WSABUF wsaBuf;
} PER_IO_DATA, * LPPER_IO_DATA;

unsigned int __stdcall CompletionThread(LPVOID pComPort);


int main(int argc, char** argv)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fputs("WSAStartup Failed", stderr);
        fputc('\n', stderr);
        exit(1);
    }

    HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    for (int i = 0; i < SystemInfo.dwNumberOfProcessors; ++i)
        _beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);

    SOCKET hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    SOCKADDR_IN servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(8888);

    bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
    listen(hServSock, 5);

    LPPER_IO_DATA PerIoData;
    LPPER_HANDLE_DATA PerHandleData;

    int RecvBytes;
    int i, Flags;

    while (TRUE)
    {
        SOCKADDR_IN clntAddr;
        int addrLen = sizeof(clntAddr);

        SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);

        PerHandleData = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
        PerHandleData->hClntSock = hClntSock;
        memcpy(&(PerHandleData->clntAddr), &clntAddr, addrLen);

        CreateIoCompletionPort((HANDLE)hClntSock, hCompletionPort, (DWORD)PerHandleData, 0);

        PerIoData = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
        memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
        PerIoData->wsaBuf.len = BUFSIZE;
        PerIoData->wsaBuf.buf = PerIoData->buffer;
        Flags = 0;

        WSARecv(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, (LPDWORD)&RecvBytes, (LPDWORD)&Flags, &(PerIoData->overlapped), NULL);
    }

    return 0;
}

unsigned int __stdcall CompletionThread(LPVOID pComPort)
{
    HANDLE hCompletionPort = (HANDLE)pComPort;
    DWORD BytesTransferred;
    LPPER_HANDLE_DATA PerHandleData;
    LPPER_IO_DATA PerIoData;
    DWORD flags;

    while (1) {
        GetQueuedCompletionStatus(hCompletionPort, &BytesTransferred, (PULONG_PTR)&PerHandleData, (LPOVERLAPPED*)&PerIoData, INFINITE);

        if (BytesTransferred == 0)
        {
            closesocket(PerHandleData->hClntSock);
            free(PerHandleData);
            free(PerIoData);
            continue;
        }

        PerIoData->wsaBuf.buf[BytesTransferred] = '\0';
        printf("Recv[%s]\n", PerIoData->wsaBuf.buf);

        PerIoData->wsaBuf.len = BytesTransferred;
        WSASend(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, NULL, 0, NULL, NULL);

        memset(&(PerIoData->overlapped), 0, sizeof(OVERLAPPED));
        PerIoData->wsaBuf.len = BUFSIZE;
        PerIoData->wsaBuf.buf = PerIoData->buffer;

        flags = 0;

        WSARecv(PerHandleData->hClntSock, &(PerIoData->wsaBuf), 1, NULL, &flags, &(PerIoData->overlapped), NULL);
    }

    return 0;
}

void ErrorHandling(const char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}