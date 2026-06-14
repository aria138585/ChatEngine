#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

#define BUFFER_SIZE 2048
#define PORT 12000

int main()
{
    WSADATA wsaData;
    SOCKET clientSock;
    struct sockaddr_in serverAddr;
    int serverAddrLen = sizeof(serverAddr);
    char inBuffer[BUFFER_SIZE],
        outBuffer[BUFFER_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == SOCKET_ERROR)
    {
        perror("Cannot utilize Windows Socket APIs at this time.");
        return 1;
    }

    clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET)
    {
        fprintf(stderr, "Error code: %d\nSocket Creation Failed.\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("192.168.1.80");

    printf("Establishing TCP connection...\r");

    if (connect(clientSock, (struct sockaddr *)&serverAddr, serverAddrLen) == -1)
    {
        fprintf(stderr, "Error code: %d\0\nTCP Connection Failed or Timed Out.\n", WSAGetLastError());
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    printf("TCP connection established successfully!\n\n");
    fprintf(stderr, "Server: %s\nPort: %d\n\n", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

    while (1)
    {
        printf("Anything to write? [Press Enter to discard]\n");
        fscanf(stdin, "%s", outBuffer);

        if (*outBuffer == '\r' || *outBuffer == '\n')
        {
            if (recvfrom(clientSock, inBuffer, BUFFER_SIZE, 0, (struct sockaddr *)&serverAddr, &serverAddrLen) == -1)
            {
                fprintf(stderr, "Error code: %d\nReceiving Failed.\n");
                continue;
            }
            fprintf(stdout, "%s says: %s\n\n", inet_ntoa(serverAddr.sin_addr), inBuffer);
        }

        if (sendto(clientSock, outBuffer, BUFFER_SIZE, 0, (struct sockaddr *)&serverAddr, serverAddrLen) == -1)
        {
            fprintf(stderr, "Error code: %d\nSending Failed.\n");
            continue;
        }
    }

    return 0;
}
