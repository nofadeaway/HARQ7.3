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
	unsigned int count_barrier = user_n + 1, pth_now = 0; //用户数为n,n个udp,n个recv,1个ip-pkt,1个main,NONONO,主线程不需要等待

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
