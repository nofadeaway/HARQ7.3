#include "FuncHead.h"

#define SEND_SIZE 400

using namespace srslte;
using namespace srsue;

extern pthread_barrier_t barrier;
extern pthread_mutex_t pdu_gets;
extern rlc_um rlc_test[];

extern UE_FX ue_test; //map容器

bool lte_send_udp(void *ptr)
{

	int port_add = 0; //FX:7.10
	if (ptr != NULL)
	{
		port_add = *((int *)ptr);
		printf("UDP:send The port offset is %d\n", port_add);
	}
	else
	{
		printf("UDP:No port offset inport.\n");
	}
	uint16_t rnti = port_add;

	int port = atoi("6604");
	port = port + port_add;
	//create socket
	int st = socket(AF_INET, SOCK_DGRAM, 0); //int socket( int af, int type, int protocol); 使用UDP则第二个参数为（SOCK_DGRAM）
	if (st == -1)
	{
		printf("create socket failed ! error message :%s\n", strerror(errno));
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr("10.129.4.106"); //目的实际地址

	uint8_t *payload_back;
	uint8_t *payload_test = new uint8_t[10240];
	uint32_t pdu_sz_test = 1500; //下面其实应该发送最终打包长度吧,待修改
	uint32_t tx_tti_test = 1;
	uint32_t pid_test = 8; //目前暂时只有1个进程
	uint32_t pid_now = 0;

	//begin{5.29}
	uint8_t *payload_tosend;
	// bool qbuff_flag=false;   //记录 qbuff::send()返回值
	//end{5.29}

	//7.3begin{发送DCI}
	int port_a = atoi("7707"); //发送DCI端口
	port_a = port_a + port_add;
	//create socket
	int st_a = socket(AF_INET, SOCK_DGRAM, 0);
	if (st_a == -1)
	{
		printf("create socket failed ! error message :%s\n", strerror(errno));
	}

	struct sockaddr_in addr_a;
	memset(&addr_a, 0, sizeof(addr_a));
	addr_a.sin_family = AF_INET;
	addr_a.sin_port = htons(port_a);
	addr_a.sin_addr.s_addr = inet_addr("10.129.4.106"); //目的实际地址
	//7.3end{发送DCI}
	//sleep(1);
	char temp_DCI[100];
	D_DCI DCI_0;

	uint32_t unread = 0;
	uint32_t subframe_now = 0;
	bool flag =true;

	ue_test.schedule_rlc_scan();

	unread = ue_test.schedule_request(rnti);
	if (!unread)
	{
		printf("No unread in rlc buffer!\n");
		flag =false;
		goto END;
	}
	else
	{
		printf("%d unread in rlc buffer!\n", unread);
	}

	memset(payload_test, 0, 10240 * sizeof(uint8_t));

	payload_back = ue_test.UE[rnti].ue_mux_test.pdu_get(payload_test, pdu_sz_test);

	printf("Now %d:::rlc_in is %d.\n", rnti, rlc_test[rnti].n_unread());

	ue_test.UE[rnti].pdu_in(payload_back, pdu_sz_test);

	payload_tosend = ue_test.UE[rnti].harq_busy(&pid_now, pdu_sz_test);

	//FX   发送DCI

	DCI_0.N_pid_now = pid_now;

	memset(temp_DCI, 0, sizeof(temp_DCI));
	memcpy(temp_DCI, &DCI_0, sizeof(D_DCI));
	if (sendto(st_a, temp_DCI, sizeof(DCI_0), 0, (struct sockaddr *)&addr_a, sizeof(addr_a)) == -1)
	{
		printf("RNTI:%d:::DCI:sendto failed ! error message :%s\n", rnti, strerror(errno));
		ue_test.UE[rnti].reset(pid_now);
		flag =false;
		goto END;
	}
	else
	{
		printf("RNTI:%d::: UDP trans begin! NO.%d:DCI succeed!\n", rnti, pid_now);
	}
	//end

	/*******************************************/

	if (payload_tosend == NULL)
	{
		ue_test.UE[rnti].reset(pid_now);
		printf("RNTI:%d::: This TTI no pdu to send!\n", rnti);
		flag =false;
		goto END;
	}
	if (sendto(st, payload_tosend, pdu_sz_test, 0, (struct sockaddr *)&addr,
			   sizeof(addr)) == -1)
	{
		printf("RNTI:%d::: sendto failed ! error message :%s\n", rnti, strerror(errno));
		ue_test.UE[rnti].reset(pid_now);
	}

	END:
	delete[] payload_test;
	close(st);
	close(st_a);
	return flag;
}
