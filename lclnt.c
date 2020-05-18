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
static SOCKET sd = -1;			// ͨѶsocket 
static struct sockaddr serv_addr;	// ��������ַ
static socklen_t serv_alen;		// ��ַ�ĳ��� 
static char ticket_buf[128];		// ����ticket�Ļ����� 
static int have_ticket = 0;			// ��ȡticketʱ���� 

#define MSGLEN		128		// ���ݱ��ĳ���
#define SERVER_PORTNUM	2020		// �������Ķ˿ں�
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

//Internet�׽��ֵ�ַ�Ĺ��캯����ʹ���������Ͷ˿�
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
	printf("������룺%d\n", WSAGetLastError());
	sprintf(buf, "CLIENT [%d]: %s", pid, msg1);
	perror(buf);
}

void setup()
{
	char hostname[BUFSIZ];
	pid = getpid();				// for ticks and msgs
	sd = make_dgram_client_socket();	// �����������ͨ��	

	if (sd == -1)
	{
		printf("������룺%d\n", WSAGetLastError());
		oops("Cannot create socket");
	}
		
	gethostname(hostname, HOSTLEN);         // ͬһ�����ϵķ�����
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


//�����֤��������ȡƱ֤
// Results: 0 for success, -1 for failure
int get_ticket()
{
	char* response;
	char buf[MSGLEN];

	if (have_ticket)				/* don't be greedy 	*/
		return(0);
	sprintf(buf, "HELO %d", pid);		/* ׫д����	*/

	if ((response = do_transaction(buf)) == NULL)
		return(-1);

	//������Ӧ�������Ƿ���Ʊ
	if (strncmp(response, "TICK", 4) == 0) {
		strcpy(ticket_buf, response + 5);	/* ��Ʊid */
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


//��Ʊ����������
int release_ticket()
{
	char buf[MSGLEN];
	char* response;

	if (!have_ticket)			// ûƱ	
		return(0);			// nothing to release

	sprintf(buf, "GBYE %s", ticket_buf);	// ׫д��Ϣ
	if ((response = do_transaction(buf)) == NULL)
		return(-1);

	//�����Ӧ
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
 * ��������������󲢻�ȡ��Ӧ
 * Results: ָ����Ϣ�ַ�����ָ��, or NULL for error
 *			NOTE: ���ص�ָ��ָ��̬�洢
 *			��ÿ�������ĵ��ø���.
 * ע�⣺Ҫ��ö���İ�ȫ�ԣ��뽫retaddr��serv addr���бȽ� (why?)
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
	//�õ��ظ�
	ret = recvfrom(sd, buf, MSGLEN, 0, &retaddr, &addrlen);
	if (ret == -1) {
		syserr("recvfrom");
		return(NULL);
	}

	//���ڷ�����Ϣ����
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
