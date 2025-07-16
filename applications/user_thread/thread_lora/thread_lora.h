/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thead_lora.h 
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

#ifndef __THREAD_LORA_H__
#define __THREAD_LORA_H__

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

#define RT_MUTEX_NAME_LORA              "lora"

#define RT_MQ_NAME_LORA_SEND            "mq_lora_send"
#define RT_MQ_NUM_LORA_SEND             (10)
#define RT_MQ_NAME_DATA_PROC            "mq_data_proc"
#define RT_MQ_NUM_DATA_PROC             (10)

#define MAX_LORA_PAYLOAD_SIZE           (32)

/* message types */
#define MSG_LORA_RECV_DATA              (0x1001)
#define MSG_LORA_SEND_DATA              (0x1002)
#define MSG_ANALIZE_LIGHT_STATE         (0x1004)
#define MSG_CHECK_DEVICE_CONNECT        (0x1008)
#define MSG_LORA_SEND_TEST              (0x1010)
#define MSG_LORA_RECV_TEST              (0x1020)
#define MSG_LORA_CONFIG_NODE_BY_RANGE   (0x1040)
#define MSG_LORA_CONFIG_NODE_BY_ID      (0x1041)
#define MSG_LORA_INIT_NODE              (0x1080)
#define MSG_LORA_SEND_FEED_DOG          (0x1100)
#define MSG_DATA_PROC_FEED_DOG          (0x1200)

/* lora command key word */
#define LORA_CMD_SET_PERIOD             (0x04)
#define LORA_CMD_SET_FREQ               (0x06)
#define LORA_CMD_CONTROL_LIGHT          (0x18)
#define LORA_CMD_LIGHT_RESPONSE         (0x18)
#define LORA_CMD_INIT_PARAMS            (0x1f)
#define LORA_CMD_RECV_TEST              (0x52)
#define LORA_CMD_NODE_ENTER_CFG         (0x81)
#define LORA_CMD_NODE_ONLINE            (0xF8)
#define LORA_CMD_LIGHT_HEART_BEAT       (0xFC)
#define LORA_CMD_DETECTOR_HEART_BEAT    (0xFE)
 
/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief structure containing lora data use in rt-thread
 */
struct rt_lora_pkt
{
    rt_uint32_t freq_hz;                            /* central frequency of package */
    rt_int16_t  rssi;                               /* rssi on concentrator side */
    rt_int8_t   snr;                                /* snr on concentrator side */
    rt_uint8_t  datarate;                           /* current datarate (SF of LoRa) */
    rt_uint8_t  len;                                /* length of payload */
    rt_uint8_t  payload[MAX_LORA_PAYLOAD_SIZE];     /* data buffer */
};
typedef struct rt_lora_pkt* rt_lora_pkt_t;

typedef struct ipc_base stu_lora_msg;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

extern struct rt_mutex     mutex_lora;     /* lock when sending or receiving */

extern rt_thread_t         tid_lora_send;  /* lora send thread handler */
extern rt_thread_t         tid_lora_recv;  /* lora receive thread handler */
extern rt_thread_t         tid_data_proc;  /* data process thread handler */

extern rt_mq_t             mq_lora_send;   /* lora send thread message queue */
extern rt_mq_t             mq_data_proc;   /* data process thread message queue */
 
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

extern rt_uint8_t       lora_get_datarate       (rt_uint8_t dr);
extern rt_uint8_t       lora_set_datarate       (rt_uint8_t dr);
extern rt_uint32_t      get_tx_freq             (void);
extern rt_int8_t        get_tx_power            (void);
extern int              start_lora_module       (void);
extern rt_uint16_t      get_self_detector_info  (char *data);

extern void         thread_lora_send        (void* parameter);
extern void         thread_lora_recv        (void* parameter);
extern void         thread_data_process     (void* parameter);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#endif /* __THREAD_LORA_H__ */
 
/* ****************************** end of file ****************************** */
