#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 512
int main(int argc, char const* argv[])
{
    int                serverfd, clientfd;
    struct sockaddr_in server, client;
    int                len;
    int                port = 1234;
    char               buffer[BUF_SIZE];

    if (argc == 2) {
        port = atoi(argv[1]);
    }

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        perror("create socket");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    len = sizeof(server);
    if (bind(serverfd, (struct sockaddr*)&server, len) < 0) {
        perror("bind socket");
        exit(1);
    }

    if (listen(serverfd, 2048) < 0) {
        perror("listen socket");
        exit(1);
    }

    while (1) {
        len = sizeof(client);
        if ((clientfd = accept(serverfd, (struct sockaddr*)&client, &len)) <
            0) {
            perror("accept error");
            exit(4);
        }

        memset(buffer, 0, sizeof(buffer));
        int size = read(clientfd, buffer, sizeof(buffer));

        if (size < 0) {
            perror("read error");
            exit(5);
        }

        if (size == 0) {
            close(clientfd);
	    continue;
        }

        if (write(clientfd, buffer, size) < 0) {
            perror("write error");
            exit(6);
        }
        close(clientfd);
    }
    close(serverfd);
    return 0;
}
