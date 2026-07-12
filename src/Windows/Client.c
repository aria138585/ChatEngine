#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#define MAX_BUFFER_SIZE 65536

CRITICAL_SECTION screen;

struct sendParam_t
{
    SOCKET clntSock;
    char buffer[MAX_BUFFER_SIZE];
} sendParam;

struct recvParam_t
{
    SOCKET clntSock;
    struct sockaddr_in srvrAddr;
} recvParam;

struct buffer_t // under construction
{
    union
    {
        char a[MAX_BUFFER_SIZE];
        wchar_t w[MAX_BUFFER_SIZE];
    } buffer;
};

void flush_buffer(char *buf)
{
    char *c = buf;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++)
    {
        *c = '\0';
        c++;
    }
}

void check_port(int prt)
{
    if (prt < -1)
    {
        fprintf(stderr, "Invalid port '%d'\nPort value cannot be negative.\n", prt);
        exit(1);
    }
    else if (prt > 65536)
    {
        fprintf(stderr, "Invalid port '%d'\nPort value cannot be greater than 65536.\n", prt);
        exit(1);
    }
    else if (prt <= 10)
    {
        fprintf(stderr, "Inaccessible port '%d'\nPort value must not be in range 0-10.\n", prt);
        exit(1);
    }
}

int parseArg(char *arg, unsigned long *ip, int *port)
{
    int i = 0;
    char *c = arg;
    char *begin = arg;
    char *prt;
    char *ip_a;

    while (*c == ':')
    {
        if (isdigit(*c) || *c == '.')
        {
            c++;
            i++;
        }
        else
        {
            return 0;
        }
    }
    strncpy_s(ip_a, 16, begin, i);

    i = 0;
    begin = ++c;
    while (*c)
    {
        if (isdigit(*c))
        {
            c++;
            i++;
        }
        else
        {
            return 0;
        }
    }
    strncpy_s(prt, 6, begin, i);
    *port = atoi(prt);

    return 1;
}

DWORD SendTask(void *Param)
{
    struct sendParam_t *param = (struct sendParam_t *)Param;
    char in_char;
    int i = 0;
    flush_buffer(param->buffer);

    while (1)
    {
        // lock screen
        TryEnterCriticalSection(&screen);
        printf("You: ");
        LeaveCriticalSection(&screen);

        for (i = 0; i < MAX_BUFFER_SIZE - 1; i++)
        {
            // lock screen
            TryEnterCriticalSection(&screen);
            in_char = fgetc(stdin);
            printf("%c", in_char);
            param->buffer[i] = in_char;
            LeaveCriticalSection(&screen);
        }
        param->buffer[i] = '\0';

        if (send(param->clntSock, param->buffer, MAX_BUFFER_SIZE, 0) < 0)
        {
            fprintf(stderr, "Error code: %d\nSending Failed.\n");
            return 1;
        }
    }
}

DWORD ReceiveTask(void *Param)
{
    struct recvParam_t *param = (struct recvParam_t *)Param;
    char buffer[MAX_BUFFER_SIZE];
    flush_buffer(buffer);

    while (1)
    {
        if (recv(param->clntSock, buffer, MAX_BUFFER_SIZE, 0) < 0)
        {
            fprintf(stderr, "Error code: %d\nReceiving Failed.\n", WSAGetLastError());
            return 1;
        }

        // lock screen
        EnterCriticalSection(&screen);
        printf("\n[%s:%u]: %s\n\n",
               inet_ntoa(param->srvrAddr.sin_addr),
               ntohs(param->srvrAddr.sin_port),
               buffer);
        LeaveCriticalSection(&screen);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET clientSock;
    struct sockaddr_in serverAddr;
    int serverAddrLen = sizeof(serverAddr);

    unsigned long destIp;
    unsigned short destPort;

    HANDLE hThread[2];

    if (argc < 2)
    {
        char ip[16], prt_a[6];
        int prt = 0;

        printf("Type destination IPv4: [Default] ");
        fgets(ip, 16, stdin);
        printf("Type destination Port: [Default] ");
        fgets(prt_a, 6, stdin);
        printf("\n");

        if (strcmp(ip, "") == 0 || strcmp(ip, "\n") == 0)
        {
            destIp = INADDR_LOOPBACK;
        }
        else
        {
            destIp = inet_addr(ip);
        }

        if (destIp == INADDR_NONE)
        {
            fprintf(stderr, "Invalid IPv4 '%s'.\n", ip);
            return -1;
        }

        if (strcmp(prt_a, "") == 0 || strcmp(prt_a, "\n") == 0)
        {
            prt = 12000;
        }
        else
        {
            prt = atoi(prt_a);
        }

        check_port(prt);
        destPort = (unsigned short)prt;
    }
    else if (argc == 1)
    {
        int prt;

        if (parseArg(argv[1], &destIp, &prt) == 0)
        {
            perror("Argument parsing failed");
            return -1;
        }

        check_port(prt);
        destPort = (unsigned short)prt;

        if (destIp == INADDR_NONE)
        {
            perror("Invalid IPv4 Address");
            return -1;
        }
    }
    else if (argc == 3)
    {
        destIp = inet_addr(argv[1]);
        int prt = atoi(argv[2]);

        check_port(prt);
        destPort = (unsigned short)prt;

        if (destIp == INADDR_NONE)
        {
            perror("Invalid IPv4 Address");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s\n%s <Dest IPv4 Addr> <Dest Port>\n%s <Dest IPv4 Addr>:<Dest Port>", argv[0], argv[0], argv[0]);
        return -1;
    }

    // destination assignment here
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = destIp;
    serverAddr.sin_port = htons(destPort);

    printf("Destination IPv4 set: %s\n", inet_ntoa(serverAddr.sin_addr));
    printf("Destination Port set: %u\n", ntohs(serverAddr.sin_port));
    // Max status line length: 30

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR)
    {
        fprintf(stderr, "Error code: %d\nWindows Socket API failed to start.\n", WSAGetLastError());
        return -1;
    }

    clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET)
    {
        fprintf(stderr, "\nError code: %d\nSocket Creation Failed.\n");
        WSACleanup();
        return -1;
    }

    printf("Connecting...                 \r"); // OK
    if (connect(clientSock, (struct sockaddr *)&serverAddr, serverAddrLen) < 0)
    {
        fprintf(stderr, "Error code: %d\nConnection Failed.\n", WSAGetLastError());
        closesocket(clientSock);
        WSACleanup();
        return -1;
    }

    printf("Connected!                    \n\n");

    InitializeCriticalSection(&screen);

    sendParam.clntSock = clientSock;
    hThread[0] = CreateThread(
        NULL,
        0,
        SendTask,
        &sendParam,
        0,
        NULL);

    recvParam.clntSock = clientSock;
    recvParam.srvrAddr = serverAddr;
    hThread[1] = CreateThread(
        NULL,
        0,
        ReceiveTask,
        &recvParam,
        0,
        NULL);
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
}