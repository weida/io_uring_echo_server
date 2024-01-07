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

#define QUEUE_DEPTH 1024
#define MAX_SQE_PER_LOOP 128

struct request {
    int          type;
    int          client_socket;
    struct iovec iov;
};

struct io_uring ring;
int             count = 0;

int add_accept_request(int server_socket, struct sockaddr_in* client_addr,
                       socklen_t* client_addr_len)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_accept(sqe, server_socket, (struct sockaddr*)client_addr,
                         client_addr_len, 0);

    struct request* req = calloc(1, sizeof(struct request));
    req->type = ACCEPT;
    io_uring_sqe_set_data(sqe, req);
    return 0;
}

int add_read_request(int client_socket)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    struct request*      req = calloc(1, sizeof(struct request));
    req->iov.iov_base = calloc(1, BUF_SIZE);
    req->iov.iov_len = BUF_SIZE;
    req->type = READ;
    req->client_socket = client_socket;
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
    printf("%d\n", count);
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

    if (listen(serverfd, 2048) < 0) {
        perror("listen socket");
        exit(1);
    }

    signal(SIGINT, sigint_handler);
    io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

    add_accept_request(serverfd, &client_addr, &client_addr_len);

    io_uring_submit(&ring);

    while (1) {
        int submissions = 0;
        int ret = io_uring_wait_cqe(&ring, &cqe);

        while (1) {
            struct request* req = (struct request*)cqe->user_data;
            switch (req->type) {
            case ACCEPT:
                add_accept_request(serverfd, &client_addr,
                                   &client_addr_len);
                submissions += 1;
                add_read_request(cqe->res);
                free_request(req);
                submissions += 1;
                break;
            case READ:
                if (cqe->res <= 0) {
                    add_close_request(req);
                    submissions += 1;
                } else {
                    add_write_request(req);
                    add_read_request(req->client_socket);
                    submissions += 2;
                }
                break;
            case WRITE:
                break;
            case CLOSE:
                free_request(req);
                break;
            default:
                fprintf(stderr, "Unexpected req type %d\n", req->type);
                break;
            }

            io_uring_cqe_seen(&ring, cqe);

            if (io_uring_sq_space_left(&ring) < MAX_SQE_PER_LOOP) {
                break;  // the submission queue is full
            }

            ret = io_uring_peek_cqe(&ring, &cqe);
            if (ret == -EAGAIN) {
                break;  // no remaining work in completion queue
            }
        }
        if (submissions > 0) {
            count++;
            io_uring_submit(&ring);
        }
    }
    io_uring_queue_exit(&ring);
    close(serverfd);
    fprintf(stderr, "close\n");

    return 0;
}
