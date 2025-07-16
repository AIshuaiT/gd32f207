/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : application.c
 * Arthor    : Test
 * Date      : April 5th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-05      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */
 
#include <rtthread.h>

#include "board.h"
#include "wnc_data_base.h" 
#include "user_thread_cfg.h"
#include "thread_led.h"
#include "thread_sysctrl.h"
#include "embedded_flash.h"
#include "external_flash.h"
#include "pcf8563.h"
#include "log.h"

#ifdef RT_USING_GD_FLASH
#include "spi_flash_gd.h"
#endif /* RT_USING_GD_FLASH */

#ifdef RT_USING_LWIP
#include <netif/ethernetif.h>
#include "gd32f20x_eth.h"
#include "thread_network.h"
#endif /* RT_USING_LWIP */

#ifdef RT_USING_LORA 
#include "thread_lora.h"
#endif /* RT_USING_LORA */

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

/* mutexs names */
#define RT_MUTEX_NAME_DETECTOR_LIST   "detector_list"
#define RT_MUTEX_NAME_RELATION_LIST   "relation_list"
#define RT_MUTEX_NAME_LIGHT_LIST      "light_list"
#define RT_MUTEX_NAME_NEW_NODE_LIST   "new_node_list"

/* for debug */
#define DEBUG_APPLICATION    1   /* 1: debug open; 0: debug close */

#if DEBUG_APPLICATION
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(...)
#endif /* DEBUG_APPLICATION */

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

detector_info_list_t    g_detector_info_list;
relation_list_t         g_relation_list;
light_info_list_t       g_light_info_list;
new_node_list_t         g_new_node_list;
config_node_list_t      g_config_node_list;

/**
 * @brief wnc lists only write in data process thread and read by some other threads,
 *        lock the list when data process thread write lists or other threads read lists,
 *        needn't lock when data process thread read lists
 */
struct rt_mutex         mutex_detector_list;
struct rt_mutex         mutex_relation_list;
struct rt_mutex         mutex_light_list;
struct rt_mutex         mutex_new_node_list;

/* use for lwip */
int  is_dhcp_enable = 1;

 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */


static void user_params_init    (void);
static void thread_init         (void* parameter);
 
/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern void feed_dog            (void);

#ifdef RT_USING_LWIP
extern int lwip_system_init     (void);
#endif /* RT_USING_LWIP */

/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief  initailize user parameters
 */
static void user_params_init(void)
{
    /* initailize devide parameters */
    get_device_params();
    if((wnc_device.eth_mac[0] != 0x00) ||
       (wnc_device.eth_mac[1] != 0x1e) ||
       (wnc_device.eth_mac[2] != 0x38))     /* blucard MAC header */
    {
        /* not correct, use default */
        init_device_param();
    }

#ifdef RT_USING_GD_FLASH
    get_wnc_config();
#endif /*RT_USING_GD_FLASH*/
    
    is_dhcp_enable = (g_wnc_config.net_config.net_mode == DHCP_MODE);
    
    /* initailize file operate */
    file_operate_reset();
    
    /* initailize wnc lists */
    init_parking_lists();

    rt_memset(&g_new_node_list, 0, sizeof(g_new_node_list));
    rt_memset(&g_config_node_list, 0, sizeof(g_config_node_list));
}

/**
 * @brief  initailize thread entry, this thread initialize user configuration and
 *         create other user threads
 */
static void thread_init(void* parameter)
{
#ifdef RT_USING_GD_FLASH
    /* initailize external flash */
    gd_init(RT_GD_FLASH_DEVICE_NAME, "spi1_0");
    ext_flash_init(RT_GD_FLASH_DEVICE_NAME);
#endif /*RT_USING_GD_FLASH*/

    feed_dog();
    
    /* initailize user parameters */
    user_params_init();                 /* need initailized external flash first */
    
    /* initailize rtc clock */
    pcf8563_init();
    
    /* perpare to use log */
    logs_init();
    add_log("power on");                /* need rtc first */
    work_state_power_on();              /* need rtc and read parameters */
    
    /* confirm new software */
    upgrade_confirm();                  /* need init log first */
    
#ifdef RT_USING_LORA    
    /* initailize lora module */
    if(start_lora_module())
    {
        while(1); /* wait for dog */
    }
#endif /* RT_USING_LORA */
                                
#ifdef RT_USING_LWIP
    /* register eth device, must get mac address first */
    rt_register_eth_dev(wnc_device.eth_mac); 
	/* initialize lwip stack */
	/* register ethernetif device */
	eth_system_device_init();    
	/* initialize lwip system */
	lwip_system_init();
                              
#endif /* RT_USING_LWIP */

    /* initailize mutexs */
    rt_mutex_init(&mutex_detector_list, RT_MUTEX_NAME_DETECTOR_LIST, RT_IPC_FLAG_PRIO);
    rt_mutex_init(&mutex_relation_list, RT_MUTEX_NAME_RELATION_LIST, RT_IPC_FLAG_PRIO);
    rt_mutex_init(&mutex_light_list, RT_MUTEX_NAME_LIGHT_LIST, RT_IPC_FLAG_PRIO);
    rt_mutex_init(&mutex_new_node_list, RT_MUTEX_NAME_NEW_NODE_LIST, RT_IPC_FLAG_PRIO);
#ifdef RT_USING_LORA 
    rt_mutex_init(&mutex_lora, RT_MUTEX_NAME_LORA, RT_IPC_FLAG_PRIO);
#endif /* RT_USING_LORA */
    
    /* create message queue */
    mq_sys_ctrl = rt_mq_create(RT_MQ_NAME_SYS_CTRL,
                               sizeof(stu_sysctrl_msg),
                               RT_MQ_NUM_SYS_CTRL,
                               RT_IPC_FLAG_FIFO);

    mq_led      = rt_mq_create(RT_MQ_NAME_LED,
                               sizeof(stu_led_msg),
                               RT_MQ_NUM_LED,
                               RT_IPC_FLAG_FIFO);
                                
#ifdef RT_USING_LORA 
    mq_lora_send = rt_mq_create(RT_MQ_NAME_LORA_SEND,
                                sizeof(stu_lora_msg),
                                RT_MQ_NUM_LORA_SEND,
                                RT_IPC_FLAG_FIFO);
                                
    mq_data_proc = rt_mq_create(RT_MQ_NAME_DATA_PROC,
                                sizeof(stu_lora_msg),
                                RT_MQ_NUM_DATA_PROC,
                                RT_IPC_FLAG_FIFO);                                
#endif /* RT_USING_LORA */
    
    /* create user threads */
    tid_sys_ctrl = rt_thread_create(RT_THREAD_NAME_SYSCTRL,
                                    thread_system_control, 
                                    RT_NULL,
                                    RT_THREAD_STACK_SIZE_SYSCTRL, 
                                    RT_THREAD_PRIORITY_SYSCTRL, 
                                    RT_THREAD_TIME_SLICE_SYSCTRL);
    if (tid_sys_ctrl != RT_NULL) rt_thread_startup(tid_sys_ctrl); 
                                
    tid_led = rt_thread_create(RT_THREAD_NAME_LED,
                               thread_led, 
                               RT_NULL,
                               RT_THREAD_STACK_SIZE_LED, 
                               RT_THREAD_PRIORITY_LED, 
                               RT_THREAD_TIME_SLICE_LED);
    if (tid_led != RT_NULL) rt_thread_startup(tid_led);                                
                                
#ifdef RT_USING_LORA
    /* lora part */
    tid_lora_send = rt_thread_create(RT_THREAD_NAME_LORASEND,
                                     thread_lora_send, 
                                     RT_NULL,
                                     RT_THREAD_STACK_SIZE_LORASEND, 
                                     RT_THREAD_PRIORITY_LORASEND, 
                                     RT_THREAD_TIME_SLICE_LORASEND);
    if (tid_lora_send != RT_NULL) rt_thread_startup(tid_lora_send);
                                
    tid_lora_recv = rt_thread_create(RT_THREAD_NAME_LORARECV,
                                     thread_lora_recv, 
                                     RT_NULL,
                                     RT_THREAD_STACK_SIZE_LORARECV, 
                                     RT_THREAD_PRIORITY_LORARECV, 
                                     RT_THREAD_TIME_SLICE_LORARECV);
    if (tid_lora_recv != RT_NULL) rt_thread_startup(tid_lora_recv);
    
    tid_data_proc = rt_thread_create(RT_THREAD_NAME_DATAPROC,
                                     thread_data_process, 
                                     RT_NULL,
                                     RT_THREAD_STACK_SIZE_DATAPROC, 
                                     RT_THREAD_PRIORITY_DATAPROC, 
                                     RT_THREAD_TIME_SLICE_DATAPROC);
    if (tid_data_proc != RT_NULL) rt_thread_startup(tid_data_proc);
#endif /* RT_USING_LORA */

#ifdef RT_USING_LWIP
    /* udp part */
    init_udp_socket_handler();
    tid_udp_client = rt_thread_create(RT_THREAD_NAME_UDP_CLI,
                                      thread_udp_client, 
                                      RT_NULL,
                                      RT_THREAD_STACK_SIZE_UDP_CLI, 
                                      RT_THREAD_PRIORITY_UDP_CLI, 
                                      RT_THREAD_TIME_SLICE_UDP_CLI);
    if (tid_udp_client != RT_NULL) rt_thread_startup(tid_udp_client);
    
    tid_udp_server = rt_thread_create(RT_THREAD_NAME_UDP_SERV,
                                      thread_udp_server, 
                                      RT_NULL,
                                      RT_THREAD_STACK_SIZE_UDP_SERV, 
                                      RT_THREAD_PRIORITY_UDP_SERV, 
                                      RT_THREAD_TIME_SLICE_UDP_SERV);
    if (tid_udp_server != RT_NULL) rt_thread_startup(tid_udp_server);
    
    /* tcp part */
    tid_tcp_server = rt_thread_create(RT_THREAD_NAME_TCP_SERV,
                                      thread_tcp_server, 
                                      RT_NULL,
                                      RT_THREAD_STACK_SIZE_TCP_SERV, 
                                      RT_THREAD_PRIORITY_TCP_SERV, 
                                      RT_THREAD_TIME_SLICE_TCP_SERV);
    if (tid_tcp_server != RT_NULL) rt_thread_startup(tid_tcp_server);
#endif /* RT_USING_LWIP */
    
    /* exit thread when work is done, system will delete this thead automatically */
}

/**
 * @brief  This function will startup user application threads.
 */
int rt_application_init(void)
{
    rt_thread_t tid;

    tid = rt_thread_create(RT_TRHEAD_NAME_INIT,
                           thread_init, 
                           RT_NULL,
                           RT_THREAD_STACK_SIZE_INIT, 
                           RT_THREAD_PRIORITY_INIT, 
                           RT_THREAD_TIME_SLICE_INIT);
    if (tid != RT_NULL) rt_thread_startup(tid);
    
    return 0;
}
 
/* ****************************** end of file ****************************** */
