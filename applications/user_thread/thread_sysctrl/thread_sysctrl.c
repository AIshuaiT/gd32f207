/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_sysctrl.c
 * Arthor    : Test
 * Date      : Apr 25th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-25      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "thread_sysctrl.h"
#include "thread_lora.h"
#include "thread_led.h"
#include "log.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

/* timers */
#define RT_TIMER_NAME_SYS_CTRL      "sys_ctrl"
#define RT_TIMER_TIMEOUT_SYS_CTRL   ((500 * RT_TICK_PER_SECOND)/1000) /* 500ms */

/* leds */
#define set_net_led_on()            set_led(ON, 7, 6);
#define set_net_led_off()           set_led(OFF, 7, 6);

/* for debug */
#define DEBUG_SYS_CTRL   0

#if DEBUG_SYS_CTRL
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(...)
#endif /* DEBUG_SYS_CTRL */

/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

enum network_state
{
    LINK_ERROR = 0,
    LINK_GOOD,
};

const char thread_names[MAX_SOFT_DOG_NUM][16] =
{
    "lora_recv",
    "lora_send",
    "data_porc",
    "udp_serv",
};

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

rt_thread_t tid_sys_ctrl = RT_NULL;
rt_mq_t     mq_sys_ctrl  = RT_NULL;

 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

volatile static rt_uint8_t   thread_soft_dogs[MAX_SOFT_DOG_NUM];
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static void         callback_timer_sysctrl  (void* parameter);
static rt_err_t     check_threads_alive     (void);
 
/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern long     list_thread         (void); 
extern void     feed_dog            (void);
extern rt_err_t network_restart     (void);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief  rt-thread timer callback functions
 * @param  parameter: rt-thread timer callback parameter
 */
static void callback_timer_sysctrl(void* parameter)
{
    stu_sysctrl_msg msg;
    static int cnt_1_sec = 1;
    
    /* feed dog */
    feed_dog();
    
    /* send message every second */
    if(cnt_1_sec)
    {
        rt_memset(&msg, 0, sizeof(msg));
        
        msg.type = MSG_SYS_1_SEC;
        
        rt_mq_send(mq_sys_ctrl, &msg, sizeof(msg));
    }
    
    /* toggle counter */
    cnt_1_sec = 1 - cnt_1_sec;
}

/**
 * @brief  check if user threads is alived
 * @retval RT_EOK means no error, others means one or more threads is dead
 */
static rt_err_t check_threads_alive(void)
{
    int i;
    rt_err_t ret = RT_EOK;
    
    for(i = 0; i < MAX_SOFT_DOG_NUM; i++)
    {
        if(thread_soft_dogs[i] == 0)
        {
            char log_str[256];
            
            rt_snprintf(log_str, sizeof(log_str), 
                        "thread %s is dead", 
                        thread_names[i]);
            add_log(log_str);
            ret = RT_ERROR;
        }
    }
    rt_memset((void*)thread_soft_dogs, 0, sizeof(thread_soft_dogs));

    return ret;
}

/**
 * @brief  request some user threads which suspend when wait messages to feed dog
 */
static void request_feed_soft_dogs(void)
{
    /* lora send thread */
    {
        stu_lora_msg msg;
        
        msg.type = MSG_LORA_SEND_FEED_DOG;
        rt_mq_send(mq_lora_send, &msg, sizeof(msg));
    }
    
    /* data process thread */
    {
        stu_lora_msg msg;
        
        msg.type = MSG_DATA_PROC_FEED_DOG;
        rt_mq_send(mq_data_proc, &msg, sizeof(msg));        
    }
}

/**
 * @brief  user threads feed soft dog to notic self is alive
 * @param  index: threads index in thread_soft_dogs
 */
void threads_feed_dog(enum threads_soft_dog index)
{
    thread_soft_dogs[index]++;
}

/**
 * @brief  system control thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_system_control(void* parameter)
{
    stu_sysctrl_msg msg;
    rt_timer_t      timer_sys_ctrl;
    int             need_reboot;
    
    int                 network_no_pack_time = 0;    
    enum network_state  net_state = LINK_ERROR;
    
    /* initailize timers */
    timer_sys_ctrl = rt_timer_create(RT_TIMER_NAME_SYS_CTRL,
                                     callback_timer_sysctrl,
                                     RT_NULL,
                                     RT_TIMER_TIMEOUT_SYS_CTRL,
                                     RT_TIMER_FLAG_PERIODIC);
    RT_ASSERT(timer_sys_ctrl != RT_NULL);
    rt_timer_start(timer_sys_ctrl);
    
    while(1)
    {
        need_reboot = 0;
        
        if(rt_mq_recv(mq_sys_ctrl, &msg, sizeof(msg), RT_WAITING_FOREVER) == RT_EOK)
        {
            switch(msg.type)
            {
            case MSG_SYS_1_SEC:
            {
                static int timer_cnt = 0;

                 ++timer_cnt;
                
                /* check network */
                if(net_state == LINK_GOOD)
                {
                    set_net_led_on();
                    network_no_pack_time++;
                    if(network_no_pack_time > 120)
                    {
                        net_state = LINK_ERROR;
                        add_log("no network data over 2 min");
                    }
                }
                else
                {
                    static int net_reset_cnt = 0;
                    
                    set_net_led_off();
                    if(++net_reset_cnt > 20)
                    {
                        net_reset_cnt = 0;
                        DEBUG_PRINTF("network restart\r\n");
                        if(network_restart() != RT_EOK)
                        {
                            DEBUG_PRINTF("network restart failed\r\n");
                            add_log("restart network failed");
                            need_reboot |= 1;
                        }
                    }
                }
                
#if DEBUG_SYS_CTRL                
                if((timer_cnt % 20) == 0)
                {
                    rt_kprintf("\r\n");
                    list_thread();
                }
#endif /* DEBUG_SYS_CTRL */

                if((timer_cnt % 20) == 0)
                {
                    request_feed_soft_dogs();
                }
                
                /* check threads every minitues */
                if((timer_cnt % 60) == 0)
                {
                    if(check_threads_alive() != RT_EOK)
                    {
                        need_reboot |= 1;
                    }
                }

                if((timer_cnt % TIME_WRITE_WORK_STATE_LOG) == 0)
                {
                    work_state_time_period();
                }                  

                break;
            }
            case MSG_SYS_USER_REBOOT:
            {
                need_reboot |= 1;
                add_log("user reboot");
                break;
            }
            case MSG_SYS_UDP_CONFIRM:
            {
                network_no_pack_time = 0;
                if(net_state != LINK_GOOD)
                {
                    add_log("network recovered");
                    net_state = LINK_GOOD;
                }
                break;
            }
            default:
                break;
            }
        }
        
        if(need_reboot)
        {
            DEBUG_PRINTF("rebooting...\r\n");
            add_log("system reboot...");
            rt_timer_stop(timer_sys_ctrl);      /* stop feed dog */
            rt_thread_delay(1000);
            while(1);   /* wait for dog */
        }
    }
}
 
/* ****************************** end of file ****************************** */
