#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "mtcp_api.h"
#include "mtcp_epoll.h"
#include "cpu.h"
#include "rss.h"
#include "http_parsing.h"
#include "netlib.h"
#include "debug.h"



int
main()
{
 struct sockaddr_in server;
 int sock;
 char buf[32];
 int n;

  // setting for mTCP
  int ret;
  mctx_t mctx;
  struct mtcp_conf mcfg;

  mtcp_getconf(&mcfg);
  mcfg.num_cores = 1;
  mtcp_setconf(&mcfg);
  ret = mtcp_init("client.conf");
  if (ret) {
    TRACE_CONFIG("Failed to initialize mtcp\n");
    abort();
    exit(EXIT_FAILURE);
  }
  mctx = mtcp_create_context(0);
  if (!mctx) {
    TRACE_ERROR("Failed to create mtcp context!\n");
    free(mctx);
    abort();
    exit(EXIT_FAILURE);
  }
  // end

 sock = socket(AF_INET, SOCK_STREAM, 0);

 server.sin_family = AF_INET;
 server.sin_port = htons(80);
 server.sin_addr.s_addr = inet_addr("172.16.2.1");

 connect(sock, (struct sockaddr *)&server, sizeof(server));

 memset(buf, 0, sizeof(buf));
 n = read(sock, buf, sizeof(buf));

 printf("%d, %s\n", n, buf);

  mtcp_destroy_context(mctx);
 close(sock);

 return 0;
}

