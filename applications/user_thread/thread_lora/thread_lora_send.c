/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_lora_send.c
 * Arthor    : Test
 * Date      : Apr 24, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-24      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "loragw_hal.h"
#include "loragw_reg.h"
#include "thread_lora.h"
#include "thread_sysctrl.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

/* for debug */
#define DEBUG_LORA_SEND     0    /* 1: debug open; 0: debug close */
#if DEBUG_LORA_SEND
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(fmt, ...)
#endif /* DEBUG_LORA_SEND */


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
 
rt_thread_t tid_lora_send = RT_NULL;
rt_mq_t     mq_lora_send  = RT_NULL;
 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static struct lgw_pkt_tx_s txpkt; /* configuration and metadata for an outbound packet */
 
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

/**
 * @brief  continuous mode sending to test tx power
 */
static void lora_send_test(void)
{
    const char * test_string = "bwnc test!\r\n";
	rt_memcpy(txpkt.payload, test_string, rt_strlen(test_string));
	
	/*add by Ann 因为SX1301的参数未能初始化，因此无线发送测试时，未能正常工作，添加下面的初始化程序 2017.11.24*/
	rt_memset(&txpkt, 0, sizeof(txpkt));
	txpkt.freq_hz  = 471100000;
	txpkt.tx_mode  = IMMEDIATE;
	txpkt.rf_power = get_tx_power();
	txpkt.modulation = MOD_LORA;
	txpkt.bandwidth = BW_125KHZ;                
	txpkt.coderate = CR_LORA_4_5;
	txpkt.preamble = 8;
	txpkt.rf_chain = 0;
	/*************************************************************************************************************/
	
	txpkt.datarate = DR_LORA_SF8;
	txpkt.size = rt_strlen(test_string);

	lgw_send(txpkt); /* non-blocking scheduling of TX packet */
}

/**
 * @brief  lora send thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_lora_send(void* parameter)
{
    stu_lora_msg  msg;
    rt_uint8_t status_var = 0;
    
    /* thread loop */
    while(1)
    {
        /* fetch messages */
        if(rt_mq_recv(mq_lora_send, &msg, sizeof(msg), RT_WAITING_FOREVER) == RT_EOK)
        {        
            switch(msg.type)
            {
            case MSG_LORA_SEND_DATA:
            {
                rt_lora_pkt_t tx_pkt = (rt_lora_pkt_t)msg.data;
                
                /* set tx packet */
                rt_memset(&txpkt, 0, sizeof(txpkt));
                txpkt.freq_hz  = tx_pkt->freq_hz;
                txpkt.tx_mode  = IMMEDIATE;
                txpkt.rf_power = get_tx_power();
                txpkt.modulation = MOD_LORA;
                txpkt.bandwidth = BW_125KHZ;                
                txpkt.coderate = CR_LORA_4_5;
                txpkt.preamble = 8;
                txpkt.rf_chain = 0;
                
                txpkt.datarate = lora_set_datarate(tx_pkt->datarate);
                txpkt.size     = tx_pkt->len;
                rt_memcpy(txpkt.payload, tx_pkt->payload, txpkt.size);
                
                /* send lora data */
                rt_mutex_take(&mutex_lora, RT_WAITING_FOREVER);
                lgw_send(txpkt); /* non-blocking scheduling of TX packet */
                rt_mutex_release(&mutex_lora);
                do {
                    rt_thread_delay(2);
                    lgw_status(TX_STATUS, &status_var); /* get TX status */
                } while (status_var != TX_FREE);
                DEBUG_PRINTF("\r\n TX done\r\n");
                
                break;
            }
            case MSG_LORA_SEND_TEST:
            {
                static rt_uint8_t in_test = 0;
                
                in_test = 1 - in_test;
                
                if(in_test)
                {
                    DEBUG_PRINTF("start lora send test...\r\n");
                    rt_mutex_take(&mutex_lora, RT_WAITING_FOREVER);
                    lgw_reg_w(LGW_TX_MODE, 1);
                    lora_send_test();
                }
                else
                {
                    lgw_reg_w(LGW_TX_MODE,0);
                    rt_mutex_release(&mutex_lora);
                    DEBUG_PRINTF("stop lora send test...\r\n");
                }
                break;
            }
            case MSG_LORA_SEND_FEED_DOG:
            {
                lora_send_feed_dog();
                break;
            }
            default:
            {
                /* wrong message type */
                DEBUG_PRINTF("get wrong message\r\n");
                break;
            }
            }
        }
    }
}
 
/* ****************************** end of file ****************************** */
