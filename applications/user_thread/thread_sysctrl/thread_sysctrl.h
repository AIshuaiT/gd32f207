/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_sysctrl.h
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

#ifndef __THREAD_SYS_CTRL_H__
#define __THREAD_SYS_CTRL_H__

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include <rtthread.h>

#include "user_ipc.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define RT_MQ_NAME_SYS_CTRL             "mq_sys_ctrl"
#define RT_MQ_NUM_SYS_CTRL              (10)

/* message types */
#define MSG_SYS_1_SEC                   (0x2001)
#define MSG_SYS_USER_REBOOT             (0x2002)
#define MSG_SYS_UDP_CONFIRM             (0x2004)

enum threads_soft_dog
{
    LORA_RECV_DOG = 0,
    LORA_SEND_DOG,
    DATA_PROC_DOG,
    UDP_CLINT_DOG,
    MAX_SOFT_DOG_NUM,
};
 
/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

typedef struct ipc_base stu_sysctrl_msg;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

extern rt_thread_t tid_sys_ctrl;    /* system control thread handler */
extern rt_mq_t     mq_sys_ctrl;     /* system control thread message queue */
 
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

 
/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern void     thread_system_control   (void* parameter); 
extern void     threads_feed_dog        (enum threads_soft_dog index);

#define lora_recv_feed_dog()        threads_feed_dog(LORA_RECV_DOG);
#define lora_send_feed_dog()        threads_feed_dog(LORA_SEND_DOG);
#define data_proc_feed_dog()        threads_feed_dog(DATA_PROC_DOG);
#define udp_clint_feed_dog()        threads_feed_dog(UDP_CLINT_DOG);

/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#endif /* __THREAD_SYS_CTRL_H__ */ 

/* ****************************** end of file ****************************** */
