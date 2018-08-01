#include "FuncHead.h"

#define SEND_SIZE 400

using namespace srslte;
using namespace srsue;



extern pthread_barrier_t barrier;
extern pthread_mutex_t pdu_gets;
extern rlc_um rlc_test[];

extern UE_FX ue_test; //map容器

void *lte_send_recv(void *ptr)
{
    bool flag;
    uint32_t subframe_now = 0;
    int flag_sub;
    ue_test.set_subframe(5);
    pthread_barrier_wait(&barrier);
    sleep(2);
    while(1)
    {
        subframe_now=ue_test.subframe_now();
        printf("\nNow subframe is NO.%d\n",subframe_now);
        flag_sub=ue_test.subframe_process_now();
        if(flag_sub==-1)
        {
           printf("Invaild subframes!\n");
        }
        else if(flag_sub==0)   //1为切换子帧
        {
            sleep(1);
        }
        else if(flag_sub==1)   //下行帧
        {
           flag = lte_send_udp(NULL);
           sleep(1);
        }
        else
        {
            flag = lte_rece(NULL);                    //上行帧
            sleep(1);
        }
        ue_test.subframe_end();
    }
}