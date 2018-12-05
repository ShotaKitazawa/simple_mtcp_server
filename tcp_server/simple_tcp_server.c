#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include "mtcp_api.h"
#include "mtcp_epoll.h"

#include "cpu.h"
#include "debug.h"
#include "http_parsing.h"
#include "netlib.h"


int
main()
{
 int sock0;
 struct sockaddr_in addr;
 struct sockaddr_in client;
 socklen_t len;
 int sock;
 int yes = 1;

 char buf[2048];
 char inbuf[2048];

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
  mctx = mtcp_create_context(0);
  if (!mctx) {
    TRACE_ERROR("Failed to create mtcp context!\n");
    free(mctx);
    abort();
    exit(EXIT_FAILURE);
  }
  // end

 sock0 = socket(AF_INET, SOCK_STREAM, 0);
 if (sock0 < 0) {
	 perror("socket");
	 return 1;
 }

 addr.sin_family = AF_INET;
 addr.sin_port = htons(12345);
 addr.sin_addr.s_addr = INADDR_ANY;

 setsockopt(sock0,
   SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

 if (bind(sock0, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
	 perror("bind");
	 return 1;
 }

 if (listen(sock0, 5) != 0) {
	 perror("listen");
	 return 1;
 }

 // 応答用HTTPメッセージ作成
 memset(buf, 0, sizeof(buf));
 snprintf(buf, sizeof(buf),
	 "HTTP/1.0 200 OK\r\n"
	 "Content-Length: 20\r\n"
	 "Content-Type: text/html\r\n"
	 "\r\n"
	 "HELLO\r\n");

 for (;;) {
   len = sizeof(client);
   sock = accept(sock0, (struct sockaddr *)&client, &len);
   //if (sock < 0) {
	 //  perror("accept");
	 //  break;
   //}

   memset(inbuf, 0, sizeof(inbuf));
   recv(sock, inbuf, sizeof(inbuf), 0);
   // 本来ならばクライアントからの要求内容をパースすべきです
   printf("%s", inbuf);

   // 相手が何を言おうとダミーHTTPメッセージ送信
   send(sock, buf, (int)strlen(buf), 0);

   close(sock);
 }

  mtcp_destroy_context(mctx);
 close(sock0);

 return 0;
}

