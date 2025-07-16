/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : user_thread_cfg.h
 * Arthor    : Test
 * Date      : Apr 7th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-07      Test          First version.
 ******************************************************************************
 */

#ifndef __USER_THREAD_CFG_H__
#define __USER_THREAD_CFG_H__

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */

/* thread name */
#define RT_TRHEAD_NAME_INIT             "init"
#define RT_THREAD_NAME_LED              "led_refresh"
#define RT_THREAD_NAME_LORASEND         "lora_send"
#define RT_THREAD_NAME_LORARECV         "lora_recv"
#define RT_THREAD_NAME_DATAPROC         "data_proc"
#define RT_THREAD_NAME_TCP_SERV         "tcp_server"
#define RT_THREAD_NAME_UDP_CLI          "udp_client"
#define RT_THREAD_NAME_UDP_SERV         "udp_server"
#define RT_THREAD_NAME_SYSCTRL          "sys_ctrl"

/* thread priority */
#define RT_THREAD_PRIORITY_INIT         (5)     /* user thread start at 5 */
#define RT_THREAD_PRIORITY_LED          (6)
#define RT_THREAD_PRIORITY_LORASEND     (7)     /* need higher than recv */
#define RT_THREAD_PRIORITY_LORARECV     (8)
#define RT_THREAD_PRIORITY_DATAPROC     (9)
#define RT_THREAD_PRIORITY_TCP_SERV     (10)
#define RT_THREAD_PRIORITY_UDP_CLI      (12)
#define RT_THREAD_PRIORITY_UDP_SERV     (13)
#define RT_THREAD_PRIORITY_SYSCTRL      (23)

/* thread stack size */
#define RT_THREAD_STACK_SIZE_INIT       (10 * 1024)     /* lgw_start need more than 8192 */
#define RT_THREAD_STACK_SIZE_LED        (512)
#define RT_THREAD_STACK_SIZE_LORASEND   (1536)
#define RT_THREAD_STACK_SIZE_LORARECV   (1024)
#define RT_THREAD_STACK_SIZE_DATAPROC   (2560)
#define RT_THREAD_STACK_SIZE_TCP_SERV   (2048)
#define RT_THREAD_STACK_SIZE_UDP_CLI    (768)
#define RT_THREAD_STACK_SIZE_UDP_SERV   (1024)
#define RT_THREAD_STACK_SIZE_SYSCTRL    (1536)

/* thread time slice */
#define RT_THREAD_TIME_SLICE_INIT       (20)    /* not very impotant because all */
                                                /* thread's priority are different */
#define RT_THREAD_TIME_SLICE_LED        (20)                                                
#define RT_THREAD_TIME_SLICE_LORASEND   (50)                                               
#define RT_THREAD_TIME_SLICE_LORARECV   (50)
#define RT_THREAD_TIME_SLICE_DATAPROC   (20)
#define RT_THREAD_TIME_SLICE_TCP_SERV   (20)
#define RT_THREAD_TIME_SLICE_UDP_CLI    (20)
#define RT_THREAD_TIME_SLICE_UDP_SERV   (20)
#define RT_THREAD_TIME_SLICE_SYSCTRL    (20)
 
 /**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

 
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
 
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#endif /* __USER_THREAD_CFG_H__ */
 
/* ****************************** end of file ****************************** */
