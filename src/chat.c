#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#define PORT 12000
#define MAX_BUFFER_SIZE 2048

struct recv_args
{
    int sz;
    int flags;
    SOCKET sock;
    void *buf;
    struct sockaddr_in inAddr;
} nec_args, *nec_args_ptr;

int WINAPI Receive(struct recv_args *);
long unsigned int Wrapper(void *);

int main(int argc, char **argv)
{
    WSADATA wsaData;
    SOCKET clientSock;
    struct sockaddr_in serverAddr;
    int serverAddrLen = sizeof(serverAddr);
    char inBuffer[MAX_BUFFER_SIZE],
        outBuffer[MAX_BUFFER_SIZE];
    int err;

    DWORD ThreadId;
    HANDLE ThreadHandle;

    printf("Preparing...\n");

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR)
    {
        fprintf(stderr, "Error code: %d\nWindows Socket API is not available.\n", WSAGetLastError());
        exit(1);
    }

    clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET)
    {
        fprintf(stderr, "Error code: %d\nSocket Creation Failed.\n", WSAGetLastError());
        WSACleanup();
        exit(1);
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("172.16.1.129");

    printf("Connecting...\n");
    err = connect(clientSock, (struct sockaddr *)&serverAddr, serverAddrLen);
    if (err == -1)
    {
        fprintf(stderr, "Error code: %d\nConnection Failed.\n", WSAGetLastError());
        closesocket(clientSock);
        WSACleanup();
        exit(1);
    }
    printf("Connected.\n");

    nec_args.sock = clientSock;
    nec_args.buf = inBuffer;
    nec_args.sz = MAX_BUFFER_SIZE;
    nec_args.flags = 0;
    nec_args.inAddr = serverAddr;
    ThreadHandle = CreateThread(
        NULL,
        0,
        Wrapper,
        &nec_args,
        0,
        &ThreadId);

    while (1)
    {
        fprintf(stdout, "You: ");
        fgets(outBuffer, MAX_BUFFER_SIZE, stdin);
        send(clientSock, outBuffer, MAX_BUFFER_SIZE, 0);
        // WaitForSingleObject(ThreadHandle, INFINITE);
    }
}

int __stdcall Receive(struct recv_args *args)
{
    int err;
    while (1)
    {
        err = recv(args->sock, args->buf, args->sz, args->flags);
        if (err < 0)
        {
            break;
        }

        fprintf(stdout, "\a\n%s:%hu says: %s\n", inet_ntoa(args->inAddr.sin_addr),
                ntohs(args->inAddr.sin_port),
                (char *)args->buf);
    }
    return err;
}

long unsigned int Wrapper(void *args)
{
    printf("Listening for incoming messages.\n");
    struct recv_args *recv_args = (struct recv_args *)args;
    int result = Receive(recv_args);
    return (long unsigned int)result;
}
