#include "FuncHead.h"

#define SEND_SIZE 400

using namespace srslte;
using namespace srsue;

extern pthread_barrier_t barrier;
extern pthread_mutex_t pdu_gets;
extern rlc_um rlc_test[];

extern UE_FX ue_test; //map容器

void *process_thistti(void *ptr)
{

    uint8_t *payload_back;
    uint8_t *payload_mux = new uint8_t[10240];
    uint32_t pdu_sz = 300; //下面其实应该发送最终打包长度吧,待修改
    uint32_t tx_tti_test = 1;
    uint32_t pid_test = 8;
    uint32_t pid_now = 0;

    uint8_t *payload_tosend;

    char temp_DCI[100];
    D_DCI DCI_0;

    uint16_t rnti=0;
    if (ptr != NULL)
    {
        rnti = *((int *)ptr);
        printf("RNTI:%d\n", rnti);
    }
    else
    {
        printf("RNTI:No input.\n");
    }

    pthread_barrier_wait(&barrier);

    while (1)
    {
        if (rlc_test[rnti].n_unread() <= 0)
        {
            usleep(5000);
            continue;
        }
        
        //Start：扫描RLC队列
        //rlc_unread(  );

        //调用调度器
        //pdusize=schedule();  得到pdu_size，得到需要发送几个TB

        memset(payload_mux, 0, 10240 * sizeof(uint8_t));
        payload_back = ue_test.UE[rnti].ue_mux_test.pdu_get(payload_mux, pdu_sz, tx_tti_test);
        printf("Now %d:::rlc_in is %d.\n", rnti, rlc_test[rnti].n_unread());
        ue_test.UE[rnti].pdu_in(payload_back, pdu_sz);
        printf("RNTI:%d:::busy_num is %d.\n", rnti, ue_test.UE[rnti].busy_num);
        payload_tosend = ue_test.UE[rnti].harq_busy(&pid_now, pdu_sz);

        //FX   发送DCI

        DCI_0.N_pid_now = pid_now;
        //发送消息队列，告知发送了DCI
        //放入共享内存

        //PDU
        //发送消息队列，告知发送PDU
        //放入共享内存
    }
    delete[] payload_mux;
}
