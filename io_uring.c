#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CLOSE 0
#define READ 1
#define WRITE 2
#define ACCEPT 3
#define BUF_SIZE 4096

#define QUEUE_DEPTH 2048

struct request {
    int          type;
    int          client_socket;
    struct iovec iov;
};

struct io_uring ring;

int add_accept_request(int server_socket, struct sockaddr_in* client_addr,
                       socklen_t* client_addr_len)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, server_socket, (struct sockaddr*)client_addr,
                         client_addr_len, 0);
    struct request* req = malloc(sizeof(*req));
    req->type = ACCEPT;
    io_uring_sqe_set_data(sqe, req);
    return 0;
}

int add_read_request(int client_socket)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    struct request*      req = malloc(sizeof(*req));
    req->iov.iov_base = malloc(BUF_SIZE);
    req->iov.iov_len = BUF_SIZE;
    req->type = READ;
    req->client_socket = client_socket;
    memset(req->iov.iov_base, 0, BUF_SIZE);
    io_uring_prep_readv(sqe, client_socket, &req->iov, 1, 0);
    io_uring_sqe_set_data(sqe, req);
    return 0;
}

int add_close_request(struct request* req)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    req->type = CLOSE;
    io_uring_prep_close(sqe, req->client_socket);
    io_uring_sqe_set_data(sqe, req);
    return 0;
}

int add_write_request(struct request* req)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    req->type = WRITE;
    io_uring_prep_writev(sqe, req->client_socket, &req->iov, 1, 0);
    io_uring_sqe_set_data(sqe, req);
    return 0;
}

int free_request(struct request* req)
{
    free(req->iov.iov_base);
    free(req);
    return 0;
}

void sigint_handler(int signo)
{
    (void)signo;
    printf("^C pressed. Shutting down.\n");
    io_uring_queue_exit(&ring);
    exit(0);
}

int main(int argc, char const* argv[])
{
    int                  serverfd, clientfd;
    struct sockaddr_in   server, client;
    int                  len;
    int                  port = 1234;
    struct io_uring_cqe* cqe;
    struct sockaddr_in   client_addr;
    socklen_t            client_addr_len = sizeof(client_addr);

    if (argc == 2) {
        port = atoi(argv[1]);
    }

    serverfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

    if (listen(serverfd, 10) < 0) {
        perror("listen socket");
        exit(1);
    }

    signal(SIGINT, sigint_handler);
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

    add_accept_request(serverfd, &client_addr, &client_addr_len);
    io_uring_submit(&ring);

    while (1) {
        int ret = io_uring_wait_cqe(&ring, &cqe);

        struct request* req = (struct request*)cqe->user_data;

        switch (req->type) {
        case ACCEPT:
            add_accept_request(serverfd, &client_addr, &client_addr_len);
            add_read_request(cqe->res);
            io_uring_submit(&ring);
            free(req);
            break;
        case READ:
            if (cqe->res <= 0) {
                add_close_request(req);
            } else {
                add_write_request(req);
            }
            io_uring_submit(&ring);
            break;
        case WRITE:
            add_read_request(req->client_socket);
            io_uring_submit(&ring);
            break;
        case CLOSE:
            free_request(req);
            break;
        default:
            fprintf(stderr, "Unexpected req type %d\n", req->type);
            break;
        }

        io_uring_cqe_seen(&ring, cqe);
    }
    io_uring_queue_exit(&ring);
    close(serverfd);
    fprintf(stderr, "close\n");

    return 0;
}
