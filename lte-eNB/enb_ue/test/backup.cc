#include "FuncHead.h"

#define SEND_SIZE 400

using namespace srslte;
using namespace srsue;



extern pthread_barrier_t barrier;
extern pthread_mutex_t pdu_gets;
extern rlc_um rlc_test[];

extern UE_FX ue_test; //map容器


void *lte_send_udp(void *ptr)
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

	
	usleep(5000);

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

	uint32_t unread=0;
	uint32_t subframe_now = 0;

	pthread_barrier_wait(&barrier);
    sleep(2);
	while (1)
	{
		
	    subframe_now =ue_test.subframe_now();   //获取当前子帧号
		while((subframe_now>0)&&(subframe_now<5))   //仅在下行子帧发送
		{
			continue;
		}
        ue_test.schedule_rlc_scan();

		unread=ue_test.schedule_request(rnti);
		if(!unread)
		{
			printf("No unread in rlc buffer!\n");
			usleep(5000);
			continue;
		}
		else{
			printf("%d unread in rlc buffer!\n",unread);
		}

		memset(payload_test,0,10240*sizeof(uint8_t));
		
		payload_back = ue_test.UE[rnti].ue_mux_test.pdu_get(payload_test, pdu_sz_test);
		
        printf("Now %d:::rlc_in is %d.\n",rnti,rlc_test[rnti].n_unread());
		
		
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
			continue;
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
			usleep(5000);

			continue;
		}
		if (sendto(st, payload_tosend, pdu_sz_test, 0, (struct sockaddr *)&addr,
				   sizeof(addr)) == -1)
		{
			printf("RNTI:%d::: sendto failed ! error message :%s\n", rnti, strerror(errno));
			ue_test.UE[rnti].reset(pid_now);
		}

		usleep(5000);
		
		ue_test.subframe_end();
	}
	delete[] payload_test;

	close(st);

	close(st_a);
}


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
extern UE_FX ue_test;    //map容器

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

void *lte_rece(void *ptr)
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
    port = port +port_add;
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

	/****************************/
	pthread_barrier_wait(&barrier);

	while (1)
	{
        subframe_now = ue_test.subframe_now();
		while((subframe_now<2)||(subframe_now>4))   //仅在上行子帧接收
		{
			continue;
		}
		if (k == 1000)
		{
			k = 0;
		}
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
			ue_test.UE[rnti].set_ack_come(subframe_now,ack_reply.ack_0);
			char str1[10] = "true", str2[10] = "false";
			printf("/******lte-Recv:");
			printf("RNTI:%d::: No.%d ACK received is %s\n",rnti, ack_reply.ACK_pid, (ack_reply.ack_0) ? str1 : str2);
		}
		k++;
		ue_test.subframe_end();
	}

END:
	close(st);
ENDD:
	close(st_a);
}


#include "FuncHead.h"
#include "FX_MAC.h"

using namespace srslte;
using namespace srsue;

pthread_t id[50]; //预留100可以创建个线程

rlc_um rlc_test[4];
demux mac_demux_test_trans; //用于发送方的,其中自动会有pdu_queue


//UE_process_FX fx_mac_test;
UE_FX ue_test; //map容器

int tun_fd; // option;全局变量--rlc写入ip时用
pthread_mutex_t pdu_gets = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier; //屏障,用于一开始线程初始化

key_t key;

int thread_create(int user_n)
{

	int temp, temp_in[200];
	int c_p_now = 0; //记录目前已经创建的线程数目
	memset(&id, 0, sizeof(id));

	if ((temp = pthread_create(&id[0], NULL, lte_send_ip_3, NULL) != 0)) //ip-pkt
		//参数：线程标识符指针 线程属性  线程运行函数起始地址  运行函数属性;创建成功返回 0
		printf("Thread 1 lte_send_ip_3 fail to create!\n");
	else
	{
		++c_p_now;
		printf("Thread 1 lte_send_ip_3 created! Now number of pthreads are %d!\n", c_p_now);
	}
	//printf("!!!");
	for (int i = 0; i < user_n; ++i)
	{
		temp_in[c_p_now] = i;
		if ((temp = pthread_create(&id[c_p_now], NULL, lte_send_recv, (void *)&(temp_in[c_p_now])) != 0)) //send-udp
			printf("Thread 2 lte_send_udp fail to create!\n");
		else
		{
			++c_p_now;
			printf("Dest:UE No.%d:Thread 2 lte_send_udp created! Now number of pthreads are %d!\n", i, c_p_now);
		}
	}
	//printf("666");
	// for (int i = 0; i < user_n; ++i)
	// {
	// 	temp_in[c_p_now] = i;
	// 	if ((temp = pthread_create(&id[c_p_now], NULL, lte_rece, (void *)&(temp_in[c_p_now])) != 0))
	// 		printf("Thread 3 lte_rece fail to create!\n");
	// 	else
	// 	{
	// 		++c_p_now;
	// 		printf("UE No.%d:Thread 3 lte_rece created! Now number of pthreads are %d!\n", i, c_p_now);
	// 	}
	// }
	return c_p_now;
}

void thread_wait(unsigned int now, int user_n)
{
	for (int i = 0; i < now; ++i)
	{
		if (id[i] != 0)
		{
			pthread_join(id[i], NULL); //等待线程结束,使用此函数对创建的线程资源回收
			if (i < 1)
			{
				printf("Thread1 lte_send_ip_3 completed!\n");
			}
			if ((i > 0) && (i < (user_n + 1)))
			{
				printf("Thread2 lte_send_udp completed!\n");
			}
			if (i >= (user_n + 1))
			{
				printf("Thread3 lte_rece completed!\n");
			}
		}
	}
}

/**************************************************************************
* main
**************************************************************************/
int main(void)
{
	/******************************************
	* tun_alloc
	******************************************/
	char tun_name[IFNAMSIZ];
	int flags = IFF_TUN; //或者IFF_TAP

	// Connect to the device
	strcpy(tun_name, "tun3");
	tun_fd = tun_alloc(tun_name, flags | IFF_NO_PI); // tun interface
	if (tun_fd < 0)
	{
		perror("Allocating interface");
		exit(1);
	}

	srslte::log_stdout log3("RLC_UM_3");

	log3.set_level(srslte::LOG_LEVEL_DEBUG);

	log3.set_hex_limit(-1);

	rlc_um_toinit rlc_um_init_visual;
	mac_dummy_timers timers_test;

	LIBLTE_RRC_RLC_CONFIG_STRUCT cnfg;
	cnfg.rlc_mode = LIBLTE_RRC_RLC_MODE_UM_BI;
	cnfg.dl_um_bi_rlc.t_reordering = LIBLTE_RRC_T_REORDERING_MS5;
	cnfg.dl_um_bi_rlc.sn_field_len = LIBLTE_RRC_SN_FIELD_LENGTH_SIZE10;
	cnfg.ul_um_bi_rlc.sn_field_len = LIBLTE_RRC_SN_FIELD_LENGTH_SIZE10;
	//mac_dummy_timers timers;
	for (int i = 0; i < 4; ++i)
	{
		rlc_test[i].init(&log3, 3, &rlc_um_init_visual, &rlc_um_init_visual, &timers_test); //LCID=3!!!!!!
		rlc_test[i].configure(&cnfg);
	}

	srslte::log_stdout log5("FX_MAC");
	log5.set_level(srslte::LOG_LEVEL_DEBUG);
	log5.set_hex_limit(-1);
	phy_interface_mac phy_interface_mac_test;
	//rlc_mac_link rlc_test;
	rlc_mac_link_group rlc_test_g;
	rlc_test_g.init();
	bsr_proc bsr_test;
	phr_proc phr_test;
	pdu_queue::process_callback *callback_test; //
	callback_test = &mac_demux_test_trans;		// 5.23
    
	ue_test.init();
	rlc_test_g.init();
	
    uint16_t user_n = 1;
	int err = -1;											  //用户数目
	unsigned int count_barrier = user_n * 2 + 1, pth_now = 0; //用户数为n,n个udp,n个recv,1个ip-pkt,1个main,NONONO,主线程不需要等待

	for (uint16_t i = 0; i < user_n; ++i)
	{
		rlc_test_g.set_link(i,&rlc_test[i]);
	}

	ue_test.rlc_all = &rlc_test_g;
	
	for (uint16_t i = 0; i < user_n; ++i)
	{
		ue_test.UE[i].rnti = i;
		ue_test.UE[i].init(&phy_interface_mac_test, &rlc_test_g.N[i], &log5, &bsr_test, &phr_test, callback_test);
	}

	//共享内存申请，信号量，队列初始化

	//线程创建

	err = pthread_barrier_init(&barrier, NULL, count_barrier);
	if (err == 0)
		printf("Barrier init succeed!\n");
	printf("Main fuction,creating thread...\n");
	pth_now = thread_create(user_n);
	printf("Main: Now %d pthread runs!\n", pth_now);
	pthread_barrier_wait(&barrier);
	printf("Main fuction, waiting for the pthread end!\n");
	thread_wait(pth_now, user_n);
	return (0);
}
