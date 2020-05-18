#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<stdlib.h>

#ifdef _WIN32
#include<ws2tcpip.h>
#elif linux
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<netdb.h>
#define SOCKET int
#endif

#pragma comment(lib,"ws2_32.lib")

#define	MSGLEN		128
//#define RECLAIM_INTERVAL 5		//每60秒回收一次
#define SERVER_PORTNUM	2020		//服务器的端口号 
#define MSGLEN		128		//数据报的长度
#define TICKET_AVAIL	0		//插槽可供使用
#define MAXUSERS	3		//只有3个用户
#define	oops(x)	{ perror(x); exit(-1); }
#define RECLAIM_INTERVAL 60		//每60秒过期一次
#define HOSTLEN  256


int ticket_array[MAXUSERS];	//票据数组
SOCKET sd = -1;
int num_tickets_out = 0;	//未付票数

//dgram.c
SOCKET make_dgram_server_socket(int portnum);
int make_internet_address(char* hostname, int port, struct sockaddr_in* addrp);
int get_internet_address(char* host, int len, int* portp, struct sockaddr_in* addrp);
//lsrv_funcs.c
SOCKET setup();
void free_all_tickets();
void shut_down();
void handle_request(char* req, struct sockaddr* client, socklen_t addlen);
//static char* do_validate(char* msg);
char* do_hello(char* msg_p);
char* do_goodbye(char* msg_p);
void narrate(char* msg1, char* msg2, struct sockaddr_in* clientp);
//void ticket_reclaim();


int main(int ac, char* av[])
{
	struct sockaddr client_addr;
	socklen_t addrlen;
	char buf[MSGLEN];
	SOCKET sock;
	int ret;
	//int	  ret, sock;
	//void	  ticket_reclaim();	//version 2 addition
	//unsigned  time_left;

	sock = setup();
	//signal(SIGALRM, ticket_reclaim); /* 跑票机 */
	//alarm(RECLAIM_INTERVAL);	 /* 在这次延误之后  */

	while (1) {
		addrlen = sizeof(client_addr);
		ret = recvfrom(sock, buf, MSGLEN, 0,
			(struct sockaddr*) & client_addr, &addrlen);
		if (ret != -1) {
			buf[ret] = '\0';
			narrate("GOT:", buf, &client_addr);
			//time_left = alarm(0);
			handle_request(buf, &client_addr, addrlen);
			//alarm(time_left);
		}
		else if (errno != EINTR)
			perror("recvfrom");
	}
}

SOCKET make_dgram_server_socket(int portnum)
{
	struct  sockaddr_in   saddr;   //在这里建立地址
	char	hostname[HOSTLEN];     //address
	SOCKET sock_id = -1;
	WORD wVersionRequested = MAKEWORD(1, 1);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) == 0)
		sock_id = socket(PF_INET, SOCK_DGRAM, 0);  // get a socket
	if (sock_id == -1) {
		printf("错误代码：%d\n",WSAGetLastError());
		return -1;
	}

	// 生成地址并将其绑定到套接字
	gethostname(hostname, HOSTLEN);         // where am I ?
	make_internet_address(hostname, portnum, &saddr);
	if (bind(sock_id, (struct sockaddr*) & saddr, sizeof(saddr)) != 0)
		return -1;
	return sock_id;
}

//Internet套接字地址的构造函数，使用主机名和端口
int make_internet_address(char* hostname, int port, struct sockaddr_in* addrp)
{
	struct hostent* hp;
#ifdef _WIN32
	memset((void*)addrp, 0, sizeof(struct sockaddr_in));
#elif linux
	bzero((void*)addrp, sizeof(struct sockaddr_in));
#endif 
	hp = gethostbyname(hostname);
	if (hp == NULL) return -1;
#ifdef _WIN32
	memcpy((void*)&addrp->sin_addr, (void*)hp->h_addr, hp->h_length);
#elif linux
	bcopy((void*)hp->h_addr, (void*)&addrp->sin_addr, hp->h_length);
#endif
	addrp->sin_port = htons(port);
	addrp->sin_family = AF_INET;
	return 0;
}

//从internet套接字地址提取主机和端口
int get_internet_address(char* host, int len, int* portp, struct sockaddr_in* addrp)
{
	strncpy(host, inet_ntoa(addrp->sin_addr), len);
	*portp = ntohs(addrp->sin_port);
	return 0;
}

//初始化许可证服务器
SOCKET setup()
{
	sd = make_dgram_server_socket(SERVER_PORTNUM);
	if (sd == -1)
		oops("make socket");
	free_all_tickets();
	return sd;
}

void free_all_tickets()
{
	int	i;
	for (i = 0; i < MAXUSERS; i++)
		ticket_array[i] = TICKET_AVAIL;
}

//关闭许可证服务器
void shut_down()
{
#ifdef _WIN32
	closesocket(sd);
#elif linux
	close(sd);
#endif
}

//请求中的代码分支
void handle_request(char* req, struct sockaddr* client, socklen_t addlen)
{
	char* response;
	int	ret;

	//做出反应
	if (strncmp(req, "HELO", 4) == 0)
		response = do_hello(req);
	else if (strncmp(req, "GBYE", 4) == 0)
		response = do_goodbye(req);
	//else if (strncmp(req, "VALD", 4) == 0)
		//response = do_validate(req);
	else
		response = "FAIL invalid request";

	// 将响应发送到客户端
	narrate("SAID:", response, client);
	ret = sendto(sd, response, strlen(response), 0, client, addlen);
	if (ret == -1)
		perror("SERVER sendto failed");
}

/****************************************************************************
 * do_validate
 * Validate client's ticket
 * IN  msg_p			message received from client
 * Results: ptr to response
 */

/*static char* do_validate(char* msg)
{
	int pid, slot;          // components of ticket 
	// msg looks like VAD pid.slot - parse it and validate 
	if (sscanf(msg + 5, "%d.%d", &pid, &slot) == 2 && ticket_array[slot] == pid)
		return("GOOD Valid ticket");

	// bad ticket 
	narrate("Bogus ticket", msg + 5, NULL);
	return("FAIL invalid ticket");
}*/

//如果有的话，给出一张票
//NOTE: 返回位于静态缓冲区中，每次调用都会覆盖它
char* do_hello(char* msg_p)
{
	int x;
	static char replybuf[MSGLEN];

	if (num_tickets_out >= MAXUSERS)
		return("FAIL no tickets available");

	//否则找张免费票给客户
	for (x = 0; x < MAXUSERS && ticket_array[x] != TICKET_AVAIL; x++)
		;
	//A sanity check - should never happen
	if (x == MAXUSERS) {
		narrate("database corrupt", "", NULL);
		return("FAIL database corrupt");
	}
	// 找到一张免费票。在数组中记录用户（pid）的“名称”。
	 // 生成表单的票证：pid.slot

	ticket_array[x] = atoi(msg_p + 5); //get pid in msg
	sprintf(replybuf, "TICK %d.%d", ticket_array[x], x);
	num_tickets_out++;
	return(replybuf);
}

//取回票客户正在返回
char* do_goodbye(char* msg_p)
{
	int pid, slot;		// components of ticket

	// 用户给了我们一张票。首先我们要把票从留言里拿出来:
	if ((sscanf((msg_p + 5), "%d.%d", &pid, &slot) != 2) ||
		(ticket_array[slot] != pid)) {
		narrate("Bogus ticket", msg_p + 5, NULL);
		return("FAIL invalid ticket");
	}

	// The ticket is valid.  Release it.
	ticket_array[slot] = TICKET_AVAIL;
	num_tickets_out--;

	// Return response
	return("THNX See ya!");
} 

// narrate() - chatty news for debugging and logging purposes
void narrate(char* msg1, char* msg2, struct sockaddr_in* clientp)
{
	fprintf(stderr, "SERVER: %s %s ", msg1, msg2);
	if (clientp)
		fprintf(stderr, "(%s : %d)", inet_ntoa(clientp->sin_addr),
			ntohs(clientp->sin_port));
	putc('\n', stderr);
}

/*
 * 检查所有票据并收回属于dead processes的票据
 * Results: none

void ticket_reclaim()
{
	int	i;
	char	tick[BUFSIZ];

	for (i = 0; i < MAXUSERS; i++) {
		if ((ticket_array[i] != TICKET_AVAIL) &&
			(kill(ticket_array[i], 0) == -1) && (errno == ESRCH)) {
			// Process is gone - free up slot
			sprintf(tick, "%d.%d", ticket_array[i], i);
			narrate("freeing", tick, NULL);
			ticket_array[i] = TICKET_AVAIL;
			num_tickets_out--;
		}
	}
	alarm(RECLAIM_INTERVAL);	// reset alarm clock
} */