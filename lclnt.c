#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<stdlib.h>
#ifdef _WIN32
#include<ws2tcpip.h>
#elif linux
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#define SOCKET int
#endif

#pragma comment(lib,"ws2_32.lib")
static int pid = -1;			// PID 
static SOCKET sd = -1;			// 通讯socket 
static struct sockaddr serv_addr;	// 服务器地址
static socklen_t serv_alen;		// 地址的长度 
static char ticket_buf[128];		// 保留ticket的缓冲区 
static int have_ticket = 0;			// 获取ticket时设置 

#define MSGLEN		128		// 数据报的长度
#define SERVER_PORTNUM	2020		// 服务器的端口号
#define	HOSTLEN		512
#define oops(p) { perror(p); exit(1) ; }

void do_regular_work();
//dgram.c
SOCKET make_dgram_client_socket();
int make_internet_address(char* hostname, int port, struct sockaddr_in* addrp);
//lclnt_funcs.c
void narrate(char* msg1, char* msg2);
void syserr(char* msg1);
void setup();
void shut_down();
int get_ticket();
int release_ticket();
char* do_transaction(char* msg);
//int validate_ticket();

int main(int ac, char* av[])
{
	setup();
	if (get_ticket() != 0)
		exit(0);

	do_regular_work();
	release_ticket();
	shut_down();
}

void do_regular_work()
{
	//printf("SuperSleep version 1.0 Running - Licensed Software\n");
	/*Sleep(15);	 //our patented sleep algorithm 
	if (validate_ticket() != 0) {
		printf("Server errors. Please Try later.\n");
		return;
	}*/
	Sleep(10);
}

SOCKET make_dgram_client_socket()
{
	WORD wVersionRequested = MAKEWORD(1, 1);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) == 0)
		return socket(PF_INET, SOCK_DGRAM, 0);
	else return (-1);
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

void narrate(char* msg1, char* msg2)
{
	fprintf(stderr, "CLIENT [%d]: %s %s\n", pid, msg1, msg2);
}

void syserr(char* msg1)
{
	char buf[MSGLEN];
	printf("错误代码：%d\n", WSAGetLastError());
	sprintf(buf, "CLIENT [%d]: %s", pid, msg1);
	perror(buf);
}

void setup()
{
	char hostname[BUFSIZ];
	pid = getpid();				// for ticks and msgs
	sd = make_dgram_client_socket();	// 用于与服务器通话	

	if (sd == -1)
	{
		printf("错误代码：%d\n", WSAGetLastError());
		oops("Cannot create socket");
	}
		
	gethostname(hostname, HOSTLEN);         // 同一主机上的服务器
	make_internet_address(hostname, SERVER_PORTNUM, &serv_addr);
	serv_alen = sizeof(serv_addr);
}

void shut_down()
{
#ifdef _WIN32
	closesocket(sd);
#elif linux
	close(sd);
#endif
}


//从许可证服务器获取票证
// Results: 0 for success, -1 for failure
int get_ticket()
{
	char* response;
	char buf[MSGLEN];

	if (have_ticket)				/* don't be greedy 	*/
		return(0);
	sprintf(buf, "HELO %d", pid);		/* 撰写请求	*/

	if ((response = do_transaction(buf)) == NULL)
		return(-1);

	//分析响应，看看是否有票
	if (strncmp(response, "TICK", 4) == 0) {
		strcpy(ticket_buf, response + 5);	/* 抢票id */
		have_ticket = 1;			/* set this flag  */
		narrate("got ticket", ticket_buf);
		return(0);
	}

	if (strncmp(response, "FAIL", 4) == 0)
		narrate("Could not get ticket", response);
	else
		narrate("Unknown message:", response);

	return(-1);
}


//把票还给服务器
int release_ticket()
{
	char buf[MSGLEN];
	char* response;

	if (!have_ticket)			// 没票	
		return(0);			// nothing to release

	sprintf(buf, "GBYE %s", ticket_buf);	// 撰写消息
	if ((response = do_transaction(buf)) == NULL)
		return(-1);

	//检查响应
	if (strncmp(response, "THNX", 4) == 0) {
		narrate("released ticket OK", "");
		return 0;
	}

	if (strncmp(response, "FAIL", 4) == 0)
		narrate("release failed", response + 5);
	else
		narrate("Unknown message:", response);
	return(-1);
}

/*
 * 向服务器发送请求并获取响应
 * Results: 指向消息字符串的指针, or NULL for error
 *			NOTE: 返回的指针指向静态存储
 *			被每个连续的调用覆盖.
 * 注意：要获得额外的安全性，请将retaddr与serv addr进行比较 (why?)
 */
char* do_transaction(char* msg)
{
	static char buf[MSGLEN];
	struct sockaddr retaddr;
	socklen_t       addrlen;
	int ret;

	ret = sendto(sd, msg, strlen(msg), 0, &serv_addr, serv_alen);
	if (ret == -1) {
		syserr("sendto");
		return(NULL);
	}
	retaddr = serv_addr;
	addrlen = serv_alen;
	//得到回复
	ret = recvfrom(sd, buf, MSGLEN, 0, &retaddr, &addrlen);
	if (ret == -1) {
		syserr("recvfrom");
		return(NULL);
	}

	//现在返回消息本身
	return(buf);
}

/*int validate_ticket()
{
	char* response;
	char buf[MSGLEN];

	if (!have_ticket)			// bizarre 
		return(0);
	sprintf(buf, "VALD %s", ticket_buf);	// compose request	

	if ((response = do_transaction(buf)) == NULL)
		return(-1);
	narrate("Validated ticket: ", response);

	if (strncmp(response, "GOOD", 4) == 0)
		return(0);
	if (strncmp(response, "FAIL", 4) == 0) {
		have_ticket = 0;
		return(-1);
	}
	narrate("Unknown message:", response);

	return(-1);
}*/
