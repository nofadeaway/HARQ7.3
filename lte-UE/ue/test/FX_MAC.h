#ifndef FX_MAC_H
#define FX_MAC_H

#define Error(fmt, ...) log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...) log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...) log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define SEND_SIZE 400
#define HARQ_NUM 8
#define SIZE_RLC_QUEUE 24
#define MAX_USERS 20
#define NUM_SUBFRAME 10

#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <map>

#include "FuncHead.h"
#include "../hdr/upper/rlc_um.h"
#include "../hdr/common/log.h"
#include "../hdr/mac/mac.h"
#include "../hdr/common/pcap.h"

using namespace srslte;
using namespace srsue;

class mac_interface_phy_FX
{
public:
  typedef struct
  {
    uint32_t pid;
    uint32_t tti;
    uint32_t last_tti;
    bool ndi;
    bool last_ndi;
    uint32_t n_bytes;
    int rv;
    uint16_t rnti;
    bool is_from_rar;
    bool is_sps_release;
    bool has_cqi_request;
    srslte_rnti_type_t rnti_type;
    srslte_phy_grant_t phy_grant;
  } mac_grant_t;

  typedef struct
  {
    bool decode_enabled;
    int rv;
    uint16_t rnti;
    bool generate_ack;
    bool default_ack;
    // If non-null, called after tb_decoded_ok to determine if ack needs to be sent
    bool (*generate_ack_callback)(void *);
    void *generate_ack_callback_arg;
    uint8_t *payload_ptr;
    srslte_softbuffer_rx_t *softbuffer;
    srslte_phy_grant_t phy_grant;
  } tb_action_dl_t;

  typedef struct
  {
    bool tx_enabled;
    bool expect_ack;
    uint32_t rv;
    uint16_t rnti;
    uint32_t current_tx_nb;
    srslte_softbuffer_tx_t *softbuffer;
    srslte_phy_grant_t phy_grant;
    uint8_t *payload_ptr;
  } tb_action_ul_t;
};

/**************************************************************************
* rlc-class
**************************************************************************/
class rlc_um_toinit
    : public pdcp_interface_rlc,
      public rrc_interface_rlc
{
public:
  rlc_um_toinit() { n_sdus = 0; }

  // PDCP interface
  void write_pdu(uint32_t lcid, byte_buffer_t *sdu)
  {
    assert(lcid == 3);
    sdus[n_sdus++] = sdu;
  }
  void write_pdu_bcch_bch(byte_buffer_t *sdu) {}
  void write_pdu_bcch_dlsch(byte_buffer_t *sdu) {}
  void write_pdu_pcch(byte_buffer_t *sdu) {}

  // RRC interface
  void max_retx_attempted() {}

  byte_buffer_t *sdus[5];
  int n_sdus;
};

/////////////////
class rlc_mac_link
    : public rlc_interface_mac
{
public:
  uint16_t rnti;
  rlc_um *rlc;
  uint32_t get_buffer_state(uint32_t lcid)
  {
    if (lcid == 3)
    {
      return rlc->get_buffer_state();
    }
    else
      return 0;
    //else{//lcid==4以及其他
    //	return rlc4.get_buffer_state();
    //}
  }

  uint32_t get_total_buffer_state(uint32_t lcid) {}

  const static int MAX_PDU_SEGMENTS = 20;

  /* MAC calls RLC to get RLC segment of nof_bytes length.
   * Segmentation happens in this function. RLC PDU is stored in payload. */
  int read_pdu(uint32_t lcid, uint8_t *payload, uint32_t nof_bytes)
  {
    int len;
    if (lcid == 3)
    {
      len = rlc->read_pdu(payload, nof_bytes);
    }
    else
      len = 0;
    //else{//lcid==4以及其他
    //len=rlc4.read_pdu(payload,nof_bytes);printf("HERE4444\n");
    //}
    return len;
  }

  /* MAC calls RLC to push an RLC PDU. This function is called from an independent MAC thread.
	* PDU gets placed into the buffer and higher layer thread gets notified. */
  void write_pdu(uint32_t lcid, uint8_t *payload, uint32_t nof_bytes)
  {
    if (lcid == 3)
    {

      rlc->write_pdu(payload, nof_bytes);
    } //else{//lcid==4以及其他
      //rlc4.write_pdu(payload,nof_bytes);printf("HERE4444\n");
      //}
  }
  uint32_t n_unread()
  {
    return rlc->n_unread();
  }
  uint32_t n_unread_bytes()
  {
    return rlc->n_unread_bytes();
  }
};

class rlc_mac_link_group
{
public:
  rlc_mac_link N[MAX_USERS];
  int32_t n_users;

  void init()
  {
    n_users = 0;
    for (int i = 0; i < MAX_USERS; ++i)
    {
      N[i].rnti = 0;
      N[i].rlc = NULL;
    }
  }
  int set_link(uint16_t rnti_, rlc_um *rlc_)
  {
    if (n_users >= MAX_USERS)
      return -1;
    N[n_users].rnti = rnti_;
    N[n_users].rlc = rlc_;
    n_users++;
    return 0;
  }
};

//**************************MAC*************//

struct ack_time_imply_map
{
  bool valid;
  uint32_t sub_past; //之前发送的子帧号
  uint32_t harq_pid;
  uint32_t ack_come; //ACK来到时的子帧号
};

class UE_process_FX //处理之前的程序
{
public:
  uint16_t rnti;
  pthread_mutex_t ACK_LOCK;
  pthread_mutex_t busy_LOCK;
  pthread_cond_t busy_signal;
  uint32_t busy_num;
  rlc_um *rlc_k;
  //mac_dummy_timers timers_test;
  mux ue_mux_test;
  demux mac_demux_test;
  //demux mac_demux_test_trans;       //用于发送方的,其中自动会有pdu_queue
  srslte::pdu_queue pdu_queue_test; //自己添加的PDU排队缓存,目前支持的HARQ进程数最多为8,既最多缓存8个PDU队列
  bool ACK[HARQ_NUM] = {false, false, false, false, false, false, false, false};
  bool ACK_default[HARQ_NUM] = {false, false, false, false, false, false, false, false};
  bool busy_index[HARQ_NUM] = {false, false, false, false, false, false, false, false};

  ack_time_imply_map ack_map[NUM_SUBFRAME] = {}; //TDD的ACK时序关系映射

  int set_ack_map(uint32_t type = 3) //设置TDD的ACK映射，现在所用为LTE的TDD configuration 3
  {
    //return set_ack_time(type);
    ack_map[0] = {false, 0, 0, 4}; //0号下行子帧的ACK在4号上行子帧
    ack_map[1] = {false, 1, 0, 2}; //1号切换子帧的ACK在下一个帧中2好子帧回复,因为这个要到下一个帧中，另外做一个处理
    ack_map[2] = {false, 2, 0, 8}; //2号上行子帧数据的ACK在下行子帧8号回复
    ack_map[3] = {false, 3, 0, 9}; //3号上行子帧数据的ACK在下行子帧9号回复
    ack_map[4] = {false, 4, 0, 0}; //4号上行子帧数据的ACK在下行子帧0号回复
    ack_map[5] = {false, 5, 0, 2}; //5号上行子帧数据的ACK在下行子帧2号回复
    ack_map[6] = {false, 6, 0, 2}; //6号上行子帧数据的ACK在下行子帧2号回复
    ack_map[7] = {false, 7, 0, 3}; //7号上行子帧数据的ACK在下行子帧3号回复
    ack_map[8] = {false, 8, 0, 3}; //8号上行子帧数据的ACK在下行子帧3号回复
    ack_map[9] = {false, 9, 0, 4}; //9号上行子帧数据的ACK在下行子帧4号回复
  }

  // int set_ack_time(uint32_t type)
  // {
  //   //...........以后添加多种配置
  // }

  void *pdu_store(uint32_t pid_now, uint8_t *payload_back, uint32_t pdu_sz_test)
  {
    if (!payload_back)
    {
      printf("RNTI:%d:::No data to store in queue.\n", rnti);
      return NULL;
    }
    bool qbuff_flag;
    //payload_back = ue_mux_test.pdu_get(payload_test, pdu_sz_test, tx_tti_test, pid_now);
    printf("RNTI:%d:::Now this pdu belongs to HARQ NO.%d\n", rnti, pid_now);

    //begin{5.28添加}
    //if(pdu_queue_test.request_buffer(pid_now,pdu_sz_test))     //request_buffer函数返回指为qbuff中ptr指针,而在下面send中其实并不需要使用
    //{printf("PID No.%d:queue's buffer request succeeded!\n",pid_now);}

    qbuff_flag = pdu_queue_test.pdu_q[pid_now].send(payload_back, pdu_sz_test); //把payload)back存入qbuff
    if (qbuff_flag)
    {
      printf("RNTI:%d:::Succeed in sending PDU to queue's buffer!(PID:%d,rp is %d,wp is %d)\n", rnti, pid_now, pdu_queue_test.pdu_q[pid_now].rp_is(), pdu_queue_test.pdu_q[pid_now].wp_is());
    }
    else
    {
      printf("RNTI:%d:::Fail in sending PDU to queue's buffer!(PID:%d,rp is %d,wp is %d)\n", rnti, pid_now, pdu_queue_test.pdu_q[pid_now].rp_is(), pdu_queue_test.pdu_q[pid_now].wp_is());
    }
  }

  bool pdu_in(uint8_t *payload_back, uint32_t pdu_sz_test) // 将pdu存入为空闲状态的HARQ进程的队列，但只查找一次
  {
    pthread_mutex_lock(&busy_LOCK);
    bool suc = false, flag = false;
    static uint32_t pid_not_busy = 0;
    if (busy_num < HARQ_NUM) //有空闲的就存入空闲状态的
    {
      while (busy_index[pid_not_busy])
      {
        pid_not_busy = (pid_not_busy + 1) % HARQ_NUM;
        flag = true;
      }
    }
    else //无空闲的随便存
    {
      pid_not_busy = (pid_not_busy + 1) % HARQ_NUM;
    }
    if (pdu_store(pid_not_busy, payload_back, pdu_sz_test))
      suc = true;
    else
      suc = false;
    if (flag)
      pid_not_busy = 0;
    pthread_mutex_unlock(&busy_LOCK);
    return suc;
  }
  void reset(uint32_t pid)
  {
    pthread_mutex_lock(&busy_LOCK);
    busy_index[pid] = false;
    busy_num--;
    pthread_mutex_unlock(&busy_LOCK);
  }
  void ack_recv(uint32_t pid)     //PID的ACK回复被接受到，从忙状态变为空闲，但ACK的赋值操作此函数没有做
  {
    pthread_mutex_lock(&busy_LOCK);
    busy_index[pid] = false;
    busy_num--;
    pthread_cond_signal(&busy_signal);
    pthread_mutex_unlock(&busy_LOCK);
  }

  uint8_t *trans_control(uint32_t pid_now, uint32_t len)
  {
    int retrans_limit = 3;
    static int retrans_times[8] = {0};
    //begin{5.29}
    //uint8_t *payload_tosend = new uint8_t[SEND_SIZE];
    //bool qbuff_flag=false;   //记录 qbuff::send()返回值
    //end{5.29}

    printf("RNTI:%d::: Now this pdu belongs to HARQ NO.%d\n", rnti, pid_now);

    /***********************************************
	    *控制重发
	    *************************************************/
    pthread_mutex_lock(&ACK_LOCK);
    bool ACK_temp = ACK[pid_now];
    pthread_mutex_unlock(&ACK_LOCK);
    // if(pdu_queue_test.pdu_q[pid_now].isempty())         //不需要,pop函数里面,若队列空,会返回NUll
    // {
    //   printf("RNTI %d::: No PDU in queue!\n",rnti);
    //   return NULL;
    // }
    printf("wp is %d\n", pdu_queue_test.pdu_q[pid_now].wp_is());
    if (ACK_temp)
    {
      //payload_tosend = payload_back;

      pdu_queue_test.pdu_q[pid_now].release();                                                                                       //将前一个已经收到对应ACK的PDU丢弃
      printf("RNTI:%d::: Now PID No.%d:queue's No.%d buffer will be sent.\n", rnti, pid_now, pdu_queue_test.pdu_q[pid_now].rp_is()); //接收到ACK,发送下一个PDU
      return (uint8_t *)pdu_queue_test.pdu_q[pid_now].pop(&len);
    }
    else //没收到ACK,重发
    {
      //memcpy(payload_tosend, pdu_queue_test.pdu_q[pid_now].pop(pdu_sz_test,1), pdu_sz_test);
      retrans_times[pid_now]++;

      if (retrans_times[pid_now] < retrans_limit)
      {
        //uint32_t len=pdu_sz_test;
        //payload_tosend =(uint8_t*) pdu_queue_test.pdu_q[pid_now].pop(&len);   //暂时是前7个进程一直ACK为true,第8个ACK一直为false
        printf("RNTI:%d::: Now PID NO.%d:the retransmission size is %d bytes.\n", rnti, pid_now, len);
        printf("RNTI:%d::: Now retransmission of PID No.%d:queue's No.%d buffer will be sent.\n", rnti, pid_now, pdu_queue_test.pdu_q[pid_now].rp_is());

        return (uint8_t *)pdu_queue_test.pdu_q[pid_now].pop(&len);
      }
      else
      {
        retrans_times[pid_now] = 0;
        pdu_queue_test.pdu_q[pid_now].release(); //丢弃超过重发次数的PDU
        printf("RNTI:%d::: The retransmission times overflow!\n", rnti);
        return (uint8_t *)pdu_queue_test.pdu_q[pid_now].pop(&len); //返回下一个PDU的指针,超过重发次数,丢弃上一个包,发送下一个包
      }
    }
  }

  bool init(phy_interface_mac *phy_h_, rlc_interface_mac *rlc_, srslte::log *log_h_, bsr_proc *bsr_procedure_, phr_proc *phr_procedure_, pdu_queue::process_callback *callback_)
  {
    //rnti=rnti_;
    pthread_mutex_init(&ACK_LOCK, NULL);
    pthread_mutex_init(&busy_LOCK, NULL);
    pthread_cond_init(&busy_signal, NULL);
    busy_num = 0;
    ue_mux_test.init(rlc_, log_h_, bsr_procedure_, phr_procedure_);
    pdu_queue_test.init(callback_, log_h_);
    mac_demux_test.init(phy_h_, rlc_, log_h_);
    set_ack_map();
  }

  uint8_t *harq_busy(uint32_t *pid, uint32_t len) //用于物理层发送，发送一个不出于busy状态的harq进程的pdu
  {
    pthread_mutex_lock(&busy_LOCK);
    uint8_t *pay_load = NULL;
    while (busy_num >= HARQ_NUM)
    {
      pthread_cond_wait(&busy_signal, &busy_LOCK);
    }
    uint32_t pid_not_busy = 0;
    while (busy_index[pid_not_busy] || pdu_queue_test.pdu_q[pid_not_busy].isempty()) //寻找一个即不处于繁忙状态，也非空的harq进程号
    {
      pid_not_busy = (pid_not_busy + 1) % HARQ_NUM;
    }
    busy_index[pid_not_busy] = true;
    busy_num++;
    *pid = pid_not_busy;
    pay_load = trans_control(pid_not_busy, len);
    pthread_mutex_unlock(&busy_LOCK);
    return pay_load;
  }

  int set_ack_wait(uint32_t pid, uint32_t subframe_now) //发送端使用的函数，设置此发送的子帧所发送的PID号，以在收到ACK后完成映射
  {
    ack_map[subframe_now].harq_pid = pid;
    ack_map[subframe_now].valid = true;
  }

  int set_ack_come(uint32_t subframe_now) //通过映射表，找出当前这个子帧收到的ACK对应前面哪个子帧
  {
    int flag = -1;
    for (int i = 0; i < NUM_SUBFRAME; ++i)
    {
      if (ack_map[i].ack_come == subframe_now)
      {
        if (ack_map[i].valid)
        {
          ack_recv(ack_map[i].harq_pid);
          ack_map[i].valid = false;
          flag =1;
        }
      }
    }
    return flag;
  }

   int set_ack_come(uint32_t subframe_now,bool ack_) //通过映射表，找出当前这个子帧收到的ACK对应前面哪个子帧
  {
    pthread_mutex_lock(&ACK_LOCK);
    int flag = -1;
    for (int i = 0; i < NUM_SUBFRAME; ++i)
    {
      if (ack_map[i].ack_come == subframe_now)
      {
        if (ack_map[i].valid)
        {
          ack_recv(ack_map[i].harq_pid);
          ack_map[i].valid = false;
          ACK[ack_map[i].harq_pid] = ack_;
          flag =1;
        }
      }
    }
    pthread_mutex_unlock(&ACK_LOCK);
    return flag;
  }
};

//申请共享内存

//int shared_memory(key_t key, )

//调度器

struct schedule_information
{
  uint16_t rnti;
  uint32_t unread;      //需要发送给该用户的ip包个数
  uint32_t payloadsize; //rlc包大小
};
struct schedule_input
{                                          //传入调度器的数据结构
  uint32_t num_user;                       //当前数据的目的UE的个数
  std::vector<schedule_information> vec_u; //存储对应UE的rlc队列的相关信息，vector大小等于num_user
};

class UE_FX //日后用于多用户情况
{
public:
  uint32_t nof_users; //用户数
  rlc_mac_link_group *rlc_all;
  std::map<uint16_t, UE_process_FX> UE;
  schedule_information rlc_scan[MAX_USERS];
  uint32_t subframe_number;

  void init()
  {
    nof_users = 0;
    for (int i = 0; i < MAX_USERS; ++i)
    {
      rlc_scan[i] = {};
    }
    rlc_all = NULL;
    subframe_number = 0;
  }

  int set_subframe(uint32_t set_) //设置子帧号
  {
    boost::mutex::scoped_lock lock(mutex);

    subframe_number = set_;
  }

  uint32_t subframe_now() //获取当前子帧号
  {
    boost::mutex::scoped_lock lock(mutex);

    return subframe_number;
  }

  int subframe_end()  //当前子帧结束，子帧号+1
  {
    boost::mutex::scoped_lock lock(mutex);
    subframe_number = (subframe_number +1)%NUM_SUBFRAME;
  }

  int subframe_process_now() //告知调用此函数者，当前子帧属性，上行/下行/切换
  {
    boost::mutex::scoped_lock lock(mutex);
    if (subframe_number >= 10) //有效子帧号为0到9
      return -1;
    else if ((subframe_number == 0) || (subframe_number > 4)) // 0,5到9 为下行帧
    {
      return 1;  //下行子帧返回1
    }
    else if (subframe_number != 1) //1号子帧为切换帧
    {
      return 2;  //上行子帧返回2
    }
    else
      return 0;  //切换帧返回0
  }

  void schedule_rlc_scan() //扫描rlc队列
  {
    nof_users = rlc_all->n_users;
    for (int i = 0; i < nof_users; ++i)
    {
      rlc_scan[i].rnti = rlc_all->N[i].rnti;
      rlc_scan[i].unread = rlc_all->N[i].n_unread();
      rlc_scan[i].payloadsize = rlc_all->N[i].n_unread_bytes();
    }
  }

  void schedule_request()
  {
    //调度算法的实现
  }
  uint32_t schedule_request(uint16_t rnti_)
  {
    for (int i = 0; i < MAX_USERS; ++i)
    {
      if (rlc_scan[i].rnti == rnti_)
      {
        printf("RNTI:%d:::unread is %d, unread_bytes is %d.\n", rlc_scan[i].rnti, rlc_scan[i].unread, rlc_scan[i].payloadsize);
        return rlc_scan[i].unread;
      }
    }
    printf("No this rnti!\n");
    return 0;
  }

private:
  boost::mutex mutex;
};
#endif