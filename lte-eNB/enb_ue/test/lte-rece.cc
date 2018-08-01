#include "FuncHead.h"

#define PAYLOAD_SIZE 400

using namespace srslte;
using namespace srsue;

//extern demux mac_demux_test;
extern mac_dummy_timers timers_test;
//extern bool ACK[8];
//extern bool ACK[];

//extern eNB_ACK I_ACK[];

//extern UE_process_FX fx_mac_test;
extern UE_FX ue_test; //map容器

// struct A_ACK
// {
// 	uint32_t ACK_pid;
// 	bool ack_0;
// };

//pthread_mutex_t ACK_LOCK = PTHREAD_MUTEX_INITIALIZER;
extern pthread_barrier_t barrier;
/**************************************************************************
* ipsend:从tun中读数据并压入队列
**************************************************************************/

bool lte_rece(void *ptr)
{

	int port_add = 0;
	if (ptr != NULL)
	{
		port_add = *((int *)ptr);
		printf("Recv:send The port offset is %d\n", port_add);
	}
	else
	{
		printf("Recv:No port offset inport.\n");
	}

	uint16_t rnti = port_add;
	//printf("enter--lte_rece\n");

	int st = socket(AF_INET, SOCK_DGRAM, 0);
	if (st == -1)
	{
		printf("open socket failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
	int port = atoi("8808"); //接受数据的端口
	port = port + port_add;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(st, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("bind IP failed ! error message : %s\n", strerror(errno));
		exit(1);
	}

	struct sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);

	int rece_size = 300, k = 0;
	; //修改为随机啊！！！！！！！！！！！
	uint8_t rece_payload[1000][PAYLOAD_SIZE] = {0};

	/****************************/
	//ACK接受
	int st_a = socket(AF_INET, SOCK_DGRAM, 0);
	if (st_a == -1)
	{
		printf("ACK:open socket failed ! error message : %s\n", strerror(errno));
		exit(1);
	}
	int port_a = atoi("5500");
	port_a = port_a + port_add;

	struct sockaddr_in addr_a;

	addr_a.sin_family = AF_INET;
	addr_a.sin_port = htons(port_a);
	addr_a.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(st_a, (struct sockaddr *)&addr_a, sizeof(addr_a)) == -1)
	{
		printf("ACK_receive:bind IP failed ! error message : %s\n", strerror(errno));
		exit(1);
	}

	uint32_t subframe_now = 0;
	subframe_now = ue_test.subframe_now();

	/****************************/
	//FX   接受ACK
	char temp[100];
	A_ACK ack_reply;
	//ack_reply.ack_0=true;
	memset(temp, 0, sizeof(temp));

	if (recv(st_a, temp, sizeof(ack_reply), 0) == -1)
	{
		printf("ACK:recvfrom failed ! error message : %s\n", strerror(errno));
	}
	else
	{
		memcpy(&ack_reply, temp, sizeof(ack_reply));
		ue_test.UE[rnti].set_ack_come(subframe_now, ack_reply.ack_0);
		char str1[10] = "true", str2[10] = "false";
		printf("/******lte-Recv:");
		printf("RNTI:%d::: No.%d ACK received is %s\n", rnti, ack_reply.ACK_pid, (ack_reply.ack_0) ? str1 : str2);
	}

	close(st);
	close(st_a);
}
