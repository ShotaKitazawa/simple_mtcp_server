#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "mtcp_api.h"
#include "mtcp_epoll.h"

#include "cpu.h"
#include "debug.h"
#include "http_parsing.h"
#include "netlib.h"

/*----------------------------------------------------------------------------*/
#define NEVENTS 16
#define BUFSIZE 1024
/*----------------------------------------------------------------------------*/
#define MAX_FLOW_NUM (10000)
#define MAX_EVENTS (MAX_FLOW_NUM * 3)
#define HTTP_HEADER_LEN 1024
#define NAME_LIMIT 256
/*----------------------------------------------------------------------------*/
enum mystate { MYSTATE_READ = 0, MYSTATE_WRITE };
/*----------------------------------------------------------------------------*/
struct server_vars {
  char request[HTTP_HEADER_LEN];
  int recv_len;
  int request_len;
  long int total_read, total_sent;
  uint8_t done;
  uint8_t rspheader_sent;
  uint8_t keep_alive;

  int fidx;                // file cache index
  char fname[NAME_LIMIT];  // file name
  long int fsize;          // file size
};
/*----------------------------------------------------------------------------*/
struct thread_context {
  mctx_t mctx;
  int ep;
  struct server_vars *svars;
};
/*----------------------------------------------------------------------------*/

int main() {
  int sock0;
  struct sockaddr_in addr;
  struct sockaddr_in client;
  socklen_t len;
  int sock;
  int n, i;
  struct epoll_event ev, ev_ret[NEVENTS];
  int epfd;
  int nfds;
  struct clientinfo {
    int fd;
    char buf[1024];
    int n;
    int state;
  };

  // setting for mTCP
  int ret;
  mctx_t mctx;
  struct mtcp_conf mcfg;
  mtcp_getconf(&mcfg);
  mcfg.num_cores = 1;
  mtcp_setconf(&mcfg);
  ret = mtcp_init("server.conf");
  if (ret) {
    TRACE_CONFIG("Failed to initialize mtcp\n");
    abort();
    exit(EXIT_FAILURE);
  }
  struct thread_context *ctx;
  ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
  if (!ctx) {
    TRACE_ERROR("Failed to create thread context!\n");
    abort();
    exit(EXIT_FAILURE);
  }
  /* create mtcp context: this will spawn an mtcp thread */
  ctx->mctx = mtcp_create_context(1);
  if (!ctx->mctx) {
    TRACE_ERROR("Failed to create mtcp context!\n");
    free(ctx);
    abort();
    exit(EXIT_FAILURE);
  }
  ctx->ep = epoll_create(30000);
  if (ctx->ep < 0) {
    mtcp_destroy_context(ctx->mctx);
    free(ctx);
    TRACE_ERROR("Failed to create epoll descriptor!\n");
    abort();
    exit(EXIT_FAILURE);
  }
  /* allocate memory for server variables */
  ctx->svars = (struct server_vars *)calloc(10000, sizeof(struct server_vars));
  if (!ctx->svars) {
    close(ctx->ep);
    mtcp_destroy_context(ctx->mctx);
    free(ctx);
    TRACE_ERROR("Failed to create server_vars struct!\n");
    abort();
    exit(EXIT_FAILURE);
  }

  mctx = ctx->mctx;

  // create socket
  sock0 = socket(AF_INET, SOCK_STREAM, 0);
  // set socket
  addr.sin_family = AF_INET;
  addr.sin_port = htons(12345);
  addr.sin_addr.s_addr = INADDR_ANY;
  bind(sock0, (struct sockaddr *)&addr, sizeof(addr));
  listen(sock0, 5);

  epfd = ctx->ep;
  memset(&ev, 0, sizeof(ev));
  ev.events = EPOLLIN;
  ev.data.ptr = malloc(sizeof(struct clientinfo));
  if (ev.data.ptr == NULL) {
    perror("malloc");
    return 1;
  }
  memset(ev.data.ptr, 0, sizeof(struct clientinfo));
  ((struct clientinfo *)ev.data.ptr)->fd = sock0;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock0, &ev) != 0) {
    perror("epoll_ctl");
    return 1;
  }
  /* (4) */
  for (;;) {
    nfds = epoll_wait(epfd, ev_ret, NEVENTS, -1);
    if (nfds < 0) {
      perror("epoll_wait");
      return 1;
    }

    /* (5) */
    printf("after epoll_wait : nfds=%d＼n", nfds);

    for (i = 0; i < nfds; i++) {
      struct clientinfo *ci = ev_ret[i].data.ptr;
      printf("fd=%d＼n", ci->fd);

      if (ci->fd == sock0) {
        /* (6) */
        /* TCPクライアントからの接続要求を受け付ける */
        len = sizeof(client);
        sock = accept(sock0, (struct sockaddr *)&client, &len);
        if (sock < 0) {
          perror("accept");
          return 1;
        }

        printf("accept sock=%d＼n", sock);

        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLONESHOT;
        ev.data.ptr = malloc(sizeof(struct clientinfo));
        if (ev.data.ptr == NULL) {
          perror("malloc");
          return 1;
        }

        memset(ev.data.ptr, 0, sizeof(struct clientinfo));
        ((struct clientinfo *)ev.data.ptr)->fd = sock;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev) != 0) {
          perror("epoll_ctl");
          return 1;
        }

      } else {
        /* (7) */
        if (ev_ret[i].events & EPOLLIN) {
          ci->n = read(ci->fd, ci->buf, BUFSIZE);
          if (ci->n < 0) {
            perror("read");
            return 1;
          }

          ci->state = MYSTATE_WRITE;
          ev_ret[i].events = EPOLLOUT;

          if (epoll_ctl(epfd, EPOLL_CTL_MOD, ci->fd, &ev_ret[i]) != 0) {
            perror("epoll_ctl");
            return 1;
          }
        } else if (ev_ret[i].events & EPOLLOUT) {
          /* (8) */
          char str[1024];
          sprintf(str, "Hello, %s", ci->buf);
          n = write(ci->fd, str, strlen(str));
          if (n < 0) {
            perror("write");
            return 1;
          }

          if (epoll_ctl(epfd, EPOLL_CTL_DEL, ci->fd, &ev_ret[i]) != 0) {
            perror("epoll_ctl");
            return 1;
          }

          close(ci->fd);
          free(ev_ret[i].data.ptr);
        }
      }
    }
  }

  mtcp_destroy_context(mctx);
  /* (9) */
  /* listen するsocketの終了 */
  close(sock0);

  return 0;
}
