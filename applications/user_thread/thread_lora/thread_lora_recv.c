/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_lora_recv.c
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
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "loragw_hal.h"
#include "thread_lora.h"
#include "thread_sysctrl.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define NB_PKT_MAX         (8)

/* for debug */
#define DEBUG_LORA_RECV    1    /* 1: debug open; 0: debug close */
#if DEBUG_LORA_RECV
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(fmt, ...)
#endif /* DEBUG_LORA_RECV */
 
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

rt_thread_t tid_lora_recv = RT_NULL;
 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static int                  nb_pkt;
static struct lgw_pkt_rx_s  rxpkt[NB_PKT_MAX]; /* array containing inbound packets metadata */
static struct lgw_pkt_rx_s  *p; /* pointer on a RX packet */
 
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
 * @brief  lora receive thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_lora_recv(void* parameter)
{
    int i;
    
    /* thread loop */
    while(1)
    {       
        lora_recv_feed_dog();
        
        /* fetch N packets */
        rt_mutex_take(&mutex_lora, RT_WAITING_FOREVER);
        nb_pkt = lgw_receive(NB_PKT_MAX, rxpkt);
        rt_mutex_release(&mutex_lora);
        
        if(nb_pkt == LGW_HAL_ERROR)
        {
            DEBUG_PRINTF("lora hal fault \r\n");
            rt_thread_delay(10);  /* 100ms */
            continue;
        }

        /* wait a short time if no packets */
		if(nb_pkt == 0) {
            rt_thread_delay(10);  /* 100ms */
            continue;
        }
        
        /* handle received packets */
        for(i=0; i < nb_pkt; ++i) {
            p = &rxpkt[i];

#if DEBUG_LORA_RECV            
            {
                int j;

                DEBUG_PRINTF("\r\n------\r\nRcv pkt >>\r\n");
                DEBUG_PRINTF(" size:%3u", p->size);
                switch (p->datarate) {
                    case DR_LORA_SF7: DEBUG_PRINTF(" SF7"); break;
                    case DR_LORA_SF8: DEBUG_PRINTF(" SF8"); break;
                    case DR_LORA_SF9: DEBUG_PRINTF(" SF9"); break;
                    case DR_LORA_SF10: DEBUG_PRINTF(" SF10"); break;
                    case DR_LORA_SF11: DEBUG_PRINTF(" SF11"); break;
                    case DR_LORA_SF12: DEBUG_PRINTF(" SF12"); break;
                    default: DEBUG_PRINTF(" datarate?");
                }
                DEBUG_PRINTF("\r\n");
                DEBUG_PRINTF(" freq: %d\r\n", p->freq_hz);
                DEBUG_PRINTF(" RSSI:%d\r\n SNR:%d (min:%d, max:%d)\r\n payload:", (int)p->rssi, (int)p->snr, (int)p->snr_min, (int)p->snr_max);

                for (j = 0; j < p->size; ++j) {
                    DEBUG_PRINTF(" %02X", p->payload[j]);
                }
                DEBUG_PRINTF(" #\r\n");
            }
#endif /* DEBUG_LORA_RECV */

            /* only pass messages we care */
            if(p->status == STAT_CRC_OK && p->size == 12)
            {
                /* send pack informations to data process thread*/
                stu_lora_msg  msg;
                rt_lora_pkt_t rx_pkt = (rt_lora_pkt_t)msg.data;
                
                rt_memset(&msg, 0, sizeof(msg));
                
                rx_pkt->freq_hz  = p->freq_hz;
                rx_pkt->rssi     = (int16_t)p->rssi;
                rx_pkt->snr      = (int8_t)p->snr;
                rx_pkt->datarate = lora_get_datarate(p->datarate);
                rx_pkt->len      = p->size;
                rt_memcpy(rx_pkt->payload, p->payload, rx_pkt->len);
                
                msg.type = MSG_LORA_RECV_DATA;

                rt_mq_send(mq_data_proc, &msg, sizeof(msg));
            }                
        }
    }
}

 
/* ****************************** end of file ****************************** */
