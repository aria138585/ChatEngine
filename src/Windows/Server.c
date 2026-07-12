#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define MAX_BUFFER_SIZE 65536

CRITICAL_SECTION screen;

struct sendParam_t
{
    SOCKET conSock;
    char buffer[MAX_BUFFER_SIZE];
} sendParam;

struct recvParam_t
{
    SOCKET conSock;
    struct sockaddr_in clntAddr;
} recvParam;

struct buffer_t
{
    union
    {
        char a[MAX_BUFFER_SIZE];
        wchar_t *w[MAX_BUFFER_SIZE];
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
    int i = 0;
    flush_buffer(param->buffer);

    while (1)
    {
        // lock screen
        EnterCriticalSection(&screen);
        // print "You: "
        printf("You: ");
        // unlock screen
        LeaveCriticalSection(&screen);

        for (i = 0; i < MAX_BUFFER_SIZE - 1 || param->buffer[i] == 10 || param->buffer[i] == 13; i++)
        {
            // lock screen
            EnterCriticalSection(&screen);
            param->buffer[i] = fgetc(stdin);
            printf("%c", param->buffer[i]);
            // unlock screen
            LeaveCriticalSection(&screen);
        }

        if (send(param->conSock, param->buffer, MAX_BUFFER_SIZE, 0) < 0)
        {
            fprintf(stderr, "Error code: %d\nSending Failed.\n", WSAGetLastError());
            return 1;
        }
    }

    return 0;
}

DWORD ReceiveTask(void *Param)
{
    struct recvParam_t *param = (struct recvParam_t *)Param;
    char buffer[MAX_BUFFER_SIZE];
    flush_buffer(buffer);

    while (1)
    {
        if (recv(param->conSock, buffer, MAX_BUFFER_SIZE, 0) < 0)
        {
            fprintf(stderr, "Error code: %d\nReceiving Failed.\n", WSAGetLastError());
            return 1;
        }

        // lock screen
        TryEnterCriticalSection(&screen);
        printf("\n[%s:%u]: %s\n\n",
               inet_ntoa(param->clntAddr.sin_addr),
               ntohs(param->clntAddr.sin_port),
               buffer);
        LeaveCriticalSection(&screen);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET listeningSock, connectionSock;
    struct sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    unsigned long destIp;
    unsigned short destPort;

    HANDLE hThread[2];

    if (argc < 2)
    {
        char ip[16], prt_a[6];
        int prt = 0;

        printf("Type destination IPv4: [Default] ");
        fgets(ip, 15, stdin);
        printf("Type destination Port: [Default] ");
        fgets(prt_a, 5, stdin);
        printf("\n");

        if (strcmp(ip, "") == 0 || strcmp(ip, "\n") == 0)
        {
            destIp = INADDR_ANY;
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

        if (prt < -1)
        {
            fprintf(stderr, "Invalid port '%d'.\nPort value cannot be negative.\n", prt);
            return -1;
        }
        else if (prt > 65536)
        {
            fprintf(stderr, "Invalid port: '%d'.\nPort value cannot be greater than 65536.\n", prt);
            return -1;
        }
        else if (prt <= 10)
        {
            fprintf(stderr, "Inaccessible port: '%d'.\nPort value must not be in range 0-10.\n", prt);
            return -1;
        }

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

        if (prt < -1)
        {
            fprintf(stderr, "Invalid Port '%d'\nPort value cannot be negative.\n");
            return -1;
        }
        else if (prt > 65536)
        {
            fprintf(stderr, "Invalid Port '%d'\nPort value cannot be greater than 65536.\n");
            return -1;
        }
        else if (prt <= 10)
        {
            fprintf(stderr, "Invalid Port '%d'\nPort value cannot be in reserved range 0-10.\n");
            return -1;
        }
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

        if (prt < -1)
        {
            fprintf(stderr, "Invalid Port '%d'\nPort value cannot be negative.\n");
            return -1;
        }
        else if (prt > 65536)
        {
            fprintf(stderr, "Invalid Port '%d'\nPort value cannot be greater than 65536.\n");
            return -1;
        }
        else if (prt <= 10)
        {
            fprintf(stderr, "Invalid Port '%d'\nPort value cannot be in reserved range 0-10.\n");
            return -1;
        }
        destPort = (unsigned short)prt;

        if (destIp == INADDR_NONE)
        {
            perror("Invalid IPv4 Address");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s\n%s <Dest IPv4 Addr> <Dest Port>\n%s <Dest IPv4 Addr>:<Dest Port>");
        return -1;
    }

    // destination assignment here
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = destIp;
    clientAddr.sin_port = htons(destPort);

    printf("Destination IPv4 set: %s\n", inet_ntoa(clientAddr.sin_addr));
    printf("Destination Port set: %u\n", ntohs(clientAddr.sin_port));
    // Max status line length: 30

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR)
    {
        fprintf(stderr, "Error code: %d\nWindows Socket API failed to start.\n", WSAGetLastError());
        return -1;
    }

    listeningSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listeningSock == INVALID_SOCKET)
    {
        fprintf(stderr, "\nError code: %d\nSocket Creation Failed.\n");
        WSACleanup();
        return -1;
    }

    printf("Binding...                    \r"); // OK
    if (bind(listeningSock, (struct sockaddr *)&clientAddr, clientAddrLen) < 0)
    {
        fprintf(stderr, "\nError code: %d\nBinding Failed.\n\tSocket: %llu\n", WSAGetLastError(), listeningSock);
        closesocket(listeningSock);
        WSACleanup();
        return -1;
    }

    printf("Waiting for connection...     \r"); // OK
    if (listen(listeningSock, 1) < 0)
    {
        fprintf(stderr, "\nError code: %d\nListening Failed.\n", WSAGetLastError());
        closesocket(listeningSock);
        WSACleanup();
        return -1;
    }

    printf("Accepting...                  \r");
    connectionSock = accept(listeningSock, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (connectionSock < 0)
    {
        fprintf(stderr, "\nError code: %d\nAccepting Failed.\n", WSAGetLastError());
        closesocket(listeningSock);
        WSACleanup();
        return -1;
    }

    printf("Connected!                    \n\n");

    InitializeCriticalSection(&screen);

    sendParam.conSock = connectionSock;
    hThread[0] = CreateThread(
        NULL,
        0,
        SendTask,
        &sendParam,
        0,
        NULL);

    recvParam.conSock = connectionSock;
    recvParam.clntAddr = clientAddr;
    hThread[1] = CreateThread(
        NULL,
        0,
        ReceiveTask,
        &recvParam,
        0,
        NULL);
    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
}