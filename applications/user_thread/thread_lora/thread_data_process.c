/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_data_process.c
 * Arthor    : Test.
 * Date      : Apr 21th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-21      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "wnc_data_base.h"
#include "loragw_hal.h"
#include "thread_led.h"
#include "thread_lora.h"
#include "thread_sysctrl.h"
#include "log.h"

#include <lwip/sockets.h>
#include "thread_network.h"
#include "embedded_flash.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define NEW_DEVICE_REPORT_TIMES         (3)

#define MAX_LORA_PAYLOAD_LEN            (256)

/* timers */
#define RT_TIMER_NAME_ANALYZE_LIGHT     "analyze_light"
#define RT_TIMER_TIMEOUT_ANALYZE_LIGHT  (1 * RT_TICK_PER_SECOND)  // 1s
#define RT_TIMER_NAME_DEV_CONNECT       "dev_connect"
#define RT_TIMER_TIMEOUT_DEV_CONNECT    (60 * RT_TICK_PER_SECOND) // 60s

enum work_mode
{
    MODE_NORMAL = 0,
    MODE_RECV_TEST,
    MODE_CONFIG_NODE_BY_RANGE,
    MODE_CONFIG_NODE_BY_ID,
    MODE_INIT_NODE,
};


#define min(a, b)     (((a) < (b)) ? (a) : (b))
#define max(a, b)     min((b), (a))

/* for debug */
#define DEBUG_DATA_PROCESS    1    /* 1: debug open; 0: debug close */
#if DEBUG_DATA_PROCESS
    #define DEBUG_PRINTF            rt_kprintf
#else
    #define DEBUG_PRINTF(fmt, ...)
#endif /* DEBUG_DATA_PROCESS */
 
 /**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containing wnc work mode
 */
struct lora_mode
{
    int                 fd;             /* response socket fd  */
    rt_uint8_t          pack_sn;        /* tcp packet serial number */
    enum work_mode      mode;           /* current work mode */
};
    
/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

rt_thread_t              tid_data_proc = RT_NULL;
rt_mq_t                  mq_data_proc  = RT_NULL;

extern struct rt_mutex   mutex_detector_list;
extern struct rt_mutex   mutex_light_list;
extern struct rt_mutex   mutex_new_node_list;

 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static struct lora_mode working_mode = 
{
    0,
    1,
    MODE_NORMAL
};

static char lora_tcp_buf[MAX_TCP_DATA_LENGTH];
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static void         callback_timer_analyze_light    (void* parameter);
static void         callback_timer_dev_connect      (void* parameter);

static rt_uint16_t  crc_calculate                   (rt_uint8_t *data, rt_uint8_t len);
static rt_uint8_t   adr_control                     (rt_lora_pkt_t rx_pkt, int index);
static void         refresh_detector_info           (single_detector_info_t node_info, int index);
static void         refresh_light_info              (single_light_info_t light_info, int index);
static void         insert_new_device               (rt_uint32_t id, rt_uint8_t type);
static void         analyse_light_state             (void);
static void         analyze_lora_pkt                (rt_lora_pkt_t rx_pkt);
static void         check_lora_node_connect         (void);
static void         lora_send_tcp_data              (int fd, 
                                                     rt_uint8_t cmd, 
                                                     char *data,
                                                     rt_uint8_t pack_sn,
                                                     rt_uint16_t data_len);
static void         perpare_config_node_by_range    (int fd, char * data);
 
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
 * @brief  rt-thread timer callback functions
 * @param  parameter: rt-thread timer callback parameter
 */
static void callback_timer_analyze_light(void* parameter)
{
    stu_lora_msg  msg;
    
    rt_memset(&msg, 0, sizeof(msg));
    
    msg.type = MSG_ANALIZE_LIGHT_STATE;
    
    rt_mq_send(mq_data_proc, &msg, sizeof(msg));
}

/**
 * @brief  rt-thread timer callback functions
 * @param  parameter: rt-thread timer callback parameter
 */
static void callback_timer_dev_connect(void* parameter)
{
    stu_lora_msg  msg;
    
    rt_memset(&msg, 0, sizeof(msg));
    
    msg.type = MSG_CHECK_DEVICE_CONNECT;
    
    rt_mq_send(mq_data_proc, &msg, sizeof(msg));    
}

/**
 * @brief  calculate crc value use for lora transmit
 * @param  data: buffer containing data
 * @param  len : length of data
 * @retval crc value
 */
static rt_uint16_t crc_calculate(rt_uint8_t *data, rt_uint8_t len)
{
    rt_uint8_t  i;
    rt_uint16_t crcvalue = 0xFFFF;
    rt_uint8_t  index = 0;
    
    RT_ASSERT(data != RT_NULL);

    while(len--)
    {
      	crcvalue ^= data[index++];
      	for(i=0; i < 8; i++)
      	{
          	if(crcvalue & 0x0001)
            {
              	crcvalue = (crcvalue >> 1) ^ 0xa001;
            }
          	else
            {
              	crcvalue >>= 1;
            }
      	}
    }
    
    return crcvalue;     
}

/**
 * @brief  calculate ADR result for the node
 * @param  rx_pkt: lora rx packet containing payload and metadata
 * @param  index:  index of detector in g_detector_info_list
 * @retval ADR result, 1 for up datarate, 0 for not change
 * 动态调整射频通信的SF速率；提升车检的SF速率
 */
static rt_uint8_t adr_control(rt_lora_pkt_t rx_pkt, int index)
{
	rt_uint8_t  ret = 0;
	rt_int32_t  avg_rssi = 0;
	rt_int8_t   avg_snr  = 0;
	int         num = min(MAX_AVG_NUM, g_detector_info_list.detector_info[index].recv_num + 1);
    int         i;

	if(rx_pkt == RT_NULL)
    {
		/* invalid argument */
		return ret /* 0, no adjust */;
	}
    
    rt_mutex_take(&mutex_detector_list, RT_WAITING_FOREVER);

	/* refresh new pack */
	g_detector_info_list.detector_info[index].c_rssi[g_detector_info_list.detector_info[index].c_ptr] = rx_pkt->rssi;
	g_detector_info_list.detector_info[index].c_snr[g_detector_info_list.detector_info[index].c_ptr]  = rx_pkt->snr;
	if(++g_detector_info_list.detector_info[index].c_ptr >= MAX_AVG_NUM)
    {
		g_detector_info_list.detector_info[index].c_ptr = 0;
	}
	g_detector_info_list.detector_info[index].current_dr = rx_pkt->datarate;

	for(i = 0; i < num; i++)
    {
		avg_rssi += g_detector_info_list.detector_info[index].c_rssi[i];
		avg_snr  += g_detector_info_list.detector_info[index].c_snr[i];
	}

	avg_rssi /= num;
	avg_snr  /= num;
	g_detector_info_list.detector_info[index].c_avg_rssi = avg_rssi;
	g_detector_info_list.detector_info[index].c_avg_snr  = avg_snr;
    
    rt_mutex_release(&mutex_detector_list);

	switch(rx_pkt->datarate) 
    {
    case 7:
        /* already the fastest daterate */
        break;
    case 8:
        ret = (rx_pkt->rssi >  -90 && rx_pkt->snr >  5) && (avg_rssi >  -90 && avg_snr >  5);
        break;
    case 9:
        ret = (rx_pkt->rssi > -100 && rx_pkt->snr >  0) && (avg_rssi > -100 && avg_snr >  0);
        break;
    case 10:
        ret = (rx_pkt->rssi > -125 && rx_pkt->snr > -5) && (avg_rssi > -125 && avg_snr > -5);
        break;
    default:
        /* invalid datarate */
        break;
	}

	return ret;
}

/**
 * @brief  refresh single detector informations in g_detector_info_list
 * @param  node_info: single detector informations need refresh
 * @param  index: index of detector in g_detector_info_list 
 */
static void refresh_detector_info(single_detector_info_t node_info, int index)
{
    char log_buf[128] = {0};
    
    rt_mutex_take(&mutex_detector_list, RT_WAITING_FOREVER);
    
    if(g_detector_info_list.detector_info[index].recv_pack_flag == 0)
    {
        g_detector_info_list.detector_info[index].recv_pack_flag = 1;
        g_detector_info_list.detector_info[index].first_cnt = node_info.cnt - 1;
        g_detector_info_list.detector_info[index].abs_recv_num++;
    }
    else
    {
        if(node_info.cnt < g_detector_info_list.detector_info[index].cnt && 
           (node_info.cnt + 256 > g_detector_info_list.detector_info[index].cnt + 1))
        {
            DEBUG_PRINTF("\r\n###############miss frame(s)!\r\n\r\n");
            rt_snprintf(log_buf, sizeof(log_buf),
                        "%d miss frame(s), lasc cnt %d, now cnt %d, SF%d, avg rssi %d, avg snr %d",
                        g_detector_info_list.detector_info[index].id,
                        g_detector_info_list.detector_info[index].cnt,
                        node_info.cnt,
                        g_detector_info_list.detector_info[index].current_dr,
                        g_detector_info_list.detector_info[index].c_avg_rssi,
                        g_detector_info_list.detector_info[index].c_avg_snr);
             add_log(log_buf);
             g_detector_info_list.detector_info[index].miss_recv_num +=
                    node_info.cnt + 256 - g_detector_info_list.detector_info[index].cnt - 1;
        }
        else if((node_info.cnt > g_detector_info_list.detector_info[index].cnt + 1))
        {
            DEBUG_PRINTF("\r\n###############miss frame(s)!\r\n\r\n");
            rt_snprintf(log_buf, sizeof(log_buf),
                        "%d miss frame(s), lasc cnt %d, now cnt %d, SF%d, avg rssi %d, avg snr %d",
                        g_detector_info_list.detector_info[index].id,
                        g_detector_info_list.detector_info[index].cnt,
                        node_info.cnt,
                        g_detector_info_list.detector_info[index].current_dr,
                        g_detector_info_list.detector_info[index].c_avg_rssi,
                        g_detector_info_list.detector_info[index].c_avg_snr);
            add_log(log_buf);
            g_detector_info_list.detector_info[index].miss_recv_num +=
                    node_info.cnt - g_detector_info_list.detector_info[index].cnt - 1;
        }

        if(node_info.cnt == g_detector_info_list.detector_info[index].cnt)
        {
            g_detector_info_list.detector_info[index].miss_send_num++;
        }
        else
        {
            g_detector_info_list.detector_info[index].abs_recv_num++;
        }


        if(node_info.cnt < g_detector_info_list.detector_info[index].cnt)
        {
            g_detector_info_list.detector_info[index].rounds++;
        }	
    }

    g_detector_info_list.detector_info[index].recv_num++;

    if(node_info.state != g_detector_info_list.detector_info[index].state)
    {
        g_detector_info_list.detector_info[index].is_park_state_changed = 1;
        g_detector_info_list.detector_info[index].change_time++;
        
        rt_snprintf(log_buf, sizeof(log_buf),
                    "%d from %d to %d, value %d",
                    node_info.id, g_detector_info_list.detector_info[index].state, node_info.state, node_info.value);
        add_log(log_buf);
        DEBUG_PRINTF("--------- %d changed park state\r\n", node_info.id);
    }				

    g_detector_info_list.detector_info[index].state    = node_info.state;
    g_detector_info_list.detector_info[index].value    = node_info.value;
    g_detector_info_list.detector_info[index].battery  = node_info.battery;

    if((node_info.battery < g_detector_info_list.detector_info[index].lowest_power) || 
       (g_detector_info_list.detector_info[index].lowest_power == 0))
    {        
        /* bettery power lower than 1.8V must got a error pack */
        if(node_info.battery < 90)
        {
            DEBUG_PRINTF("\r\n @@@@@@ %d got battery %d V\r\n\r\n",
                         node_info.id, (node_info.battery * 2));
            rt_snprintf(log_buf, sizeof(log_buf),
                        "%d got battery %d V",
                        node_info.id, (node_info.battery * 2));
            add_log(log_buf);
        }
        else
        {
            g_detector_info_list.detector_info[index].lowest_power = node_info.battery;
        }
    }

    g_detector_info_list.detector_info[index].rssi     = node_info.rssi;
    g_detector_info_list.detector_info[index].cnt      = node_info.cnt;
    g_detector_info_list.detector_info[index].snr      = node_info.snr;
    g_detector_info_list.detector_info[index].interval = 0;

    g_detector_info_list.detector_info[index].resend1 += node_info.resend1;
    g_detector_info_list.detector_info[index].resend2 += node_info.resend2;
    g_detector_info_list.detector_info[index].resend3 += node_info.resend3;

    g_detector_info_list.detector_info[index].send_num = 
                   g_detector_info_list.detector_info[index].send_num_before_this_time +
		           256 * g_detector_info_list.detector_info[index].rounds + 
				   g_detector_info_list.detector_info[index].cnt -
				   g_detector_info_list.detector_info[index].first_cnt;
    
    rt_mutex_release(&mutex_detector_list);

#if DEBUG_DATA_PROCESS
    {
        float    loss_per, detector_loss_per, abs_loss_per;

        DEBUG_PRINTF("controller avg rssi %d, avg snr %d\r\n",
                    g_detector_info_list.detector_info[index].c_avg_rssi,
                    g_detector_info_list.detector_info[index].c_avg_snr);

        loss_per = ((float)(g_detector_info_list.detector_info[index].send_num +
                   g_detector_info_list.detector_info[index].resend1 + 
                   g_detector_info_list.detector_info[index].resend2 * 2 +
                   g_detector_info_list.detector_info[index].resend3 * 3 +
                   g_detector_info_list.detector_info[index].miss_recv_num * 3 -
                   g_detector_info_list.detector_info[index].recv_num) * 100) /
                   (float)(g_detector_info_list.detector_info[index].send_num + 
                   g_detector_info_list.detector_info[index].resend1 + 
                   g_detector_info_list.detector_info[index].resend2 * 2 +
                   g_detector_info_list.detector_info[index].resend3 * 3 +
                   g_detector_info_list.detector_info[index].miss_recv_num * 3);
        abs_loss_per = (float)(g_detector_info_list.detector_info[index].send_num - 
                               g_detector_info_list.detector_info[index].abs_recv_num) * 100 /
                       (float)g_detector_info_list.detector_info[index].send_num;
        detector_loss_per = (float)g_detector_info_list.detector_info[index].miss_send_num * 100 /
                          (float)g_detector_info_list.detector_info[index].recv_num;
        DEBUG_PRINTF("recv num      : %d\r\n"
                     "abs recv num  : %d\r\n"
                     "send num      : %d\r\n"
                     "loss percent  : %d.%02d%%\r\n"
                     "abs loss      : %d.%02d%%\r\n"
                     "detector loss : %d.%02d%%\r\n"
                     "offline times : %d\r\n"
                     "reset times   : %d\r\n"
                     "resend once   : %d\r\n"
                     "resend twice  : %d\r\n"
                     "resend 3 times: %d\r\n"
                     "miss frames   : %d\r\n",
                     g_detector_info_list.detector_info[index].recv_num,
                     g_detector_info_list.detector_info[index].abs_recv_num,
                     g_detector_info_list.detector_info[index].send_num,
                     (int)(loss_per*100)/100, (int)(loss_per*100)%100,
                     (int)(abs_loss_per*100)/100, (int)(abs_loss_per*100)%100,
                     (int)(detector_loss_per*100)/100, (int)(detector_loss_per*100)%100,
                     g_detector_info_list.detector_info[index].offline_time,
                     g_detector_info_list.detector_info[index].reset_time,
                     g_detector_info_list.detector_info[index].resend1,
                     g_detector_info_list.detector_info[index].resend2,
                     g_detector_info_list.detector_info[index].resend3,
                     g_detector_info_list.detector_info[index].miss_recv_num);
    }
#endif	/* DEBUG_DATA_PROCESS */    
}

/**
 * @brief  refresh single light informations in g_light_info_list
 * @param  light_info: single light informations need refresh
 * @param  index:      index of light in g_light_info_list 
 */
static void refresh_light_info(single_light_info_t light_info, int index)
{
    rt_mutex_take(&mutex_light_list, RT_WAITING_FOREVER);
    
    g_light_info_list.light_info[index].state         = 0;
    g_light_info_list.light_info[index].rssi          = light_info.rssi;
    g_light_info_list.light_info[index].snr           = light_info.snr;
    g_light_info_list.light_info[index].current_color = light_info.current_color;
    g_light_info_list.light_info[index].interval      = 0;
    g_light_info_list.light_info[index].time_left     = 10;
    
    rt_mutex_release(&mutex_light_list);
}

/**
 * @brief  insert new LoRa device to g_new_node_list
 * @param  id:   device id
 * @param  type: device type(light or detector)
 */
static void insert_new_device(rt_uint32_t id, rt_uint8_t type)
{
    int i;
    
    rt_mutex_take(&mutex_new_node_list, RT_WAITING_FOREVER);
    
    for(i = 0; i < g_new_node_list.num; i++)
    {
        if(g_new_node_list.node[i].id == id)
        {
            g_new_node_list.node[i].time_left = NEW_DEVICE_REPORT_TIMES;     /* refresh send time */
            break;
        }
    }

    if((i >= g_new_node_list.num) && (g_new_node_list.num < MAX_NODE_WHOLE_PARKING_LOT))
    {
        DEBUG_PRINTF("get a new device\r\n");
        g_new_node_list.node[g_new_node_list.num].id          = id;
        g_new_node_list.node[g_new_node_list.num].device_type = type;
        g_new_node_list.node[g_new_node_list.num].time_left   = NEW_DEVICE_REPORT_TIMES;
        g_new_node_list.num++;
    }
    rt_mutex_release(&mutex_new_node_list);
}

/**
 * @brief  send tcp data to pc through lora thread
 * @param  fd: socket fd
 * @param  cmd: command key word
 * @param  data: pointer to data buffer
 * @param  data_len: data length
 */
static void lora_send_tcp_data(int fd, rt_uint8_t cmd, char *data, rt_uint8_t pack_sn, rt_uint16_t data_len)
{
    tcp_pack_header_t   header;
    char                *payload;
    size_t           len;
    
    header  = (tcp_pack_header_t)lora_tcp_buf;
    payload = lora_tcp_buf + sizeof(struct tcp_pack_header);
    
    header->cmd = cmd;
    header->id  = htonl(wnc_device.id);
    rt_strncpy(header->device_type, DEVICE_TYPE, strlen(DEVICE_TYPE));
    header->pack_sn = pack_sn;
    header->data_len = htons(data_len);
    rt_memcpy(payload, data, data_len);
    
    len = sizeof(struct tcp_pack_header) + data_len + 1;
    *(payload + data_len + 1) = xor_verify(data, (len - 1));
    send(fd, lora_tcp_buf, len, 0);    
}


static void config_lora_node(int fd, rt_lora_pkt_t rx_pkt, enum work_mode mode)
{
    int         i;
    uint32_t    id;
    uint16_t    crc_value;
    
    struct rt_lora_pkt  tx_pkt;
    stu_lora_msg  msg;

	id = (uint32_t)((rx_pkt->payload[0] << 24) | 
	                (rx_pkt->payload[1] << 16) |
					(rx_pkt->payload[2] << 8 ) |
					(rx_pkt->payload[3]      ));

    for(i = 0; i < g_config_node_list.num; i++)
    {
        if((id == g_config_node_list.node[i].id) &&
           (!g_config_node_list.node[i].done))
        {
            /* wait node turn to receiver */
			rt_thread_delay(5);

            switch(rx_pkt->payload[4])
            {
            case LORA_CMD_DETECTOR_HEART_BEAT:
            {
                if(rx_pkt->payload[5] == 0x0b)
                {
                    DEBUG_PRINTF("%d in config\r\n", id);
                    rt_memcpy(tx_pkt.payload, rx_pkt->payload, 10);
                    
                    switch(mode)
                    {
                    case MODE_CONFIG_NODE_BY_RANGE:
                    case MODE_CONFIG_NODE_BY_ID:
                    {
                        tx_pkt.payload[4] = LORA_CMD_SET_PERIOD;
                        tx_pkt.payload[5] = (g_config_node_list.period >> 8) & 0xff;
                        tx_pkt.payload[6] =  g_config_node_list.period & 0xff;
                        
                        break;
                    }
                    case MODE_INIT_NODE:
                    {
                        tx_pkt.payload[4] = LORA_CMD_INIT_PARAMS;
                        
                        break;
                    }
                    default:
                    {
                        /* should not catch other case */
                        break;
                    }
                    }
    				crc_value = crc_calculate(tx_pkt.payload, 10);
    				tx_pkt.payload[10] = (crc_value >> 8) & 0xff;
    				tx_pkt.payload[11] =  crc_value & 0xff;
    				tx_pkt.len         = 12;
    				tx_pkt.datarate    = rx_pkt->datarate;
                    tx_pkt.freq_hz     = rx_pkt->freq_hz;                     
                }
                else
                {
                    DEBUG_PRINTF("%d heart beat\r\n", id);
                    rt_memcpy(tx_pkt.payload, rx_pkt->payload, 10);
    				tx_pkt.payload[5] = 0x0a;
    				crc_value = crc_calculate(tx_pkt.payload, 10);
    				tx_pkt.payload[10] = (crc_value >> 8) & 0xff;
    				tx_pkt.payload[11] =  crc_value & 0xff;
    				tx_pkt.len         = 12;
    				tx_pkt.datarate    = rx_pkt->datarate;
                    tx_pkt.freq_hz     = get_tx_freq();
                }

                msg.type = MSG_LORA_SEND_DATA;
                rt_memcpy(msg.data, &tx_pkt, sizeof(tx_pkt));
                rt_mq_send(mq_lora_send, &msg, sizeof(msg));

                break;
            }
            case LORA_CMD_SET_PERIOD:
            {
                DEBUG_PRINTF("%d set period ok\r\n", id);

                memcpy(tx_pkt.payload, rx_pkt->payload, 10);
                tx_pkt.payload[4]  = LORA_CMD_SET_FREQ;
                tx_pkt.payload[5]  = (g_config_node_list.freq >> 24) & 0xff;
                tx_pkt.payload[6]  = (g_config_node_list.freq >> 16) & 0xff;
                tx_pkt.payload[7]  = (g_config_node_list.freq >> 8 ) & 0xff;
                tx_pkt.payload[8]  =  g_config_node_list.freq        & 0xff;
				crc_value = crc_calculate(tx_pkt.payload, 10);
				tx_pkt.payload[10] = (crc_value >> 8) & 0xff;
				tx_pkt.payload[11] =  crc_value & 0xff;
				tx_pkt.len         = 12;
				tx_pkt.datarate    = rx_pkt->datarate;
                tx_pkt.freq_hz     = rx_pkt->freq_hz;

                msg.type = MSG_LORA_SEND_DATA;
                rt_memcpy(msg.data, &tx_pkt, sizeof(tx_pkt));
                rt_mq_send(mq_lora_send, &msg, sizeof(msg));

                break;
            }
            case LORA_CMD_SET_FREQ:
            {
                rt_uint8_t cmd;
                
                g_config_node_list.node[i].done = 1;
                DEBUG_PRINTF("%d config done\r\n", id);
                id = ntohl(id);

                switch(mode)
                {
                case MODE_CONFIG_NODE_BY_RANGE:
                {
                    cmd = CMD_LORA_CONFIG_NODE_BY_RANGE;
                    break;
                }
                case MODE_CONFIG_NODE_BY_ID:
                {
                    cmd = CMD_LORA_CONFIG_NODE_BY_ID;
                    break;
                }
                default:
                {
                    /* should not catch other case */
                    break;
                }
                }                
                
                lora_send_tcp_data (fd, 
                                    cmd, 
                                    (char *)&id, 
                                    working_mode.pack_sn, 
                                    sizeof(id)); 
                working_mode.pack_sn++;               
                break;
            }
            case LORA_CMD_INIT_PARAMS:
            {
                g_config_node_list.node[i].done = 1;
                DEBUG_PRINTF("%d config done\r\n", id);
                id = ntohl(id);
                lora_send_tcp_data (fd, 
                                    CMD_LORA_INIT_NODE, 
                                    (char *)&id, 
                                    working_mode.pack_sn, 
                                    sizeof(id)); 
                working_mode.pack_sn++; 
                break;
            }
            default:
            {
                break;
            }
            }
        }
    }
}

/**
 * @brief  analyze received lora packet when working in special mode
 * @param  rx_pkt: rx packet containing payload and metadata
 */
// TODO : this is not a good way
static rt_uint8_t analyze_special_mode(rt_lora_pkt_t rx_pkt)
{
    if(working_mode.mode == MODE_NORMAL)
    {
        return 0;
    }
    else
    {
        char        payload[MAX_LORA_PAYLOAD_LEN];
        rt_uint16_t data_len;
        char *      data;
        
        switch(working_mode.mode)
        {
        case MODE_RECV_TEST:
        {
            if(rx_pkt->payload[4] == LORA_CMD_RECV_TEST)
            {
                rt_uint32_t tmp_freq;
                rt_int32_t  tmp_rssi;
                rt_int32_t  tmp_snr;            
                
                tmp_freq = htonl(rx_pkt->freq_hz);
                tmp_rssi = htonl(rx_pkt->rssi);
                tmp_snr  = htonl(rx_pkt->snr);
                
                data = payload;
                
                rt_memcpy(data, &tmp_freq, sizeof(tmp_freq));
                data += sizeof(tmp_freq);
                
                rt_memcpy(data, &tmp_rssi, sizeof(tmp_rssi));
                data += sizeof(tmp_rssi);
                
                rt_memcpy(data, &tmp_snr, sizeof(tmp_snr));
                data += sizeof(tmp_snr);
                
                data_len = 12;
                
                lora_send_tcp_data (working_mode.fd, 
                                    CMD_LORA_RECV_TEST, 
                                    payload, 
                                    working_mode.pack_sn, 
                                    data_len);
                 working_mode.pack_sn++;
            }
            break;
        }
        case MODE_CONFIG_NODE_BY_RANGE:
        case MODE_CONFIG_NODE_BY_ID:
        case MODE_INIT_NODE:
        {
            config_lora_node(working_mode.fd, rx_pkt, working_mode.mode);
            break;
        }
        default:
        {
            /* should not get here */
            break;
        }
        }
        return 1;
    }
}
    
/**
 * @brief  analyze received lora packet
 * @param  rx_pkt: rx packet containing payload and metadata
 */
static void analyze_lora_pkt(rt_lora_pkt_t rx_pkt)
{
    rt_uint32_t  id;
    rt_uint16_t  crc_value;
    
    if(crc_calculate(rx_pkt->payload, 10) != (rx_pkt->payload[10] << 8 | rx_pkt->payload[11]))
    {
        /* got wrong packet */
        return;
    }
    
    if(analyze_special_mode(rx_pkt))
    {
        /* working in special mode */
        return;
    }
    
    id = (rt_uint32_t) (rx_pkt->payload[0] << 24 | 
                        rx_pkt->payload[1] << 16 |
                        rx_pkt->payload[2] << 8  |
                        rx_pkt->payload[3]);
    
    switch(rx_pkt->payload[4])
    {
    case LORA_CMD_DETECTOR_HEART_BEAT:
    {
        int i;
        stu_lora_msg  msg;
        single_detector_info_t  node_info;        
        rt_uint8_t resend_times;        
        struct rt_lora_pkt  tx_pkt;
        
        rt_memset(&msg, 0, sizeof(msg));

#if DEBUG_DATA_PROCESS        
        DEBUG_PRINTF("\r\n\r\n--- receive detector heartbeat from %d\r\n", id);
        DEBUG_PRINTF("freq: %d, SF%d\r\n", rx_pkt->freq_hz, rx_pkt->datarate);
        DEBUG_PRINTF("concentrator rssi = %d, snr = %d\r\n",
                     rx_pkt->rssi,
                     rx_pkt->snr); 
        DEBUG_PRINTF("data: ");
        for(i = 0; i < 10; i++)
        {
            DEBUG_PRINTF("0x%02x ", rx_pkt->payload[i]);
        } 
        DEBUG_PRINTF("\r\n"); 
#endif /* DEBUG_DATA_PROCESS */        
        
        for(i = 0; i < g_detector_info_list.num; i++)
        {
            if(id == g_detector_info_list.detector_info[i].id)
            {
                /* wait for node turn to receiver */
                rt_thread_delay(5);

                rt_memcpy(&tx_pkt, rx_pkt, sizeof(tx_pkt));
                tx_pkt.payload[5]  = adr_control(rx_pkt, i);
                crc_value = crc_calculate(tx_pkt.payload, 10);
                tx_pkt.payload[10] = (crc_value >> 8) & 0xff;
                tx_pkt.payload[11] =  crc_value       & 0xff;
                tx_pkt.freq_hz     = get_tx_freq();
                
                msg.type = MSG_LORA_SEND_DATA;
                rt_memcpy(msg.data, &tx_pkt, sizeof(tx_pkt));
                
                /* send response first */
                rt_mq_send(mq_lora_send, &msg, sizeof(msg));
                
                /* log detector out of configuration mode */
                if(g_detector_info_list.detector_info[i].is_in_config == RT_TRUE)
                {
                    char log_buf[128] = {0};
                    
                    g_detector_info_list.detector_info[i].is_in_config = RT_FALSE;
                    rt_snprintf(log_buf, sizeof(log_buf),
                                "%d exit config",
                                g_detector_info_list.detector_info[i].id);
                    add_log(log_buf);
                }
                
                /* refresh list */                
                rt_memset(&node_info, 0, sizeof(node_info));
                node_info.id        = id;
                node_info.state     = (rx_pkt->payload[5] >> 7) & 0x01;
                node_info.value     = rx_pkt->payload[5] & 0x7f;
                node_info.battery   = rx_pkt->payload[6];
                node_info.rssi      = rx_pkt->payload[7];
                node_info.cnt       = rx_pkt->payload[8];
                node_info.snr       = (int8_t)(rx_pkt->payload[9] & 0x3f) - 20;
                node_info.interval  = 0;

                resend_times = (rx_pkt->payload[9] >> 6) & 0x03;
                
                if((rx_pkt->payload[5] >> 6) & 0x01)
                {
                    char log_buf[64];
                    rt_snprintf(log_buf, sizeof(log_buf),
                                "%d not stable", id);
                    add_log(log_buf);
                }

                switch(resend_times) {
                case 0:
                    break;
                case 1:
                    node_info.resend1 = 1;
                    break;
                case 2:
                    node_info.resend2 = 1;
                    break;
                case 3:
                    node_info.resend3 = 1;
                    break;
                default:
                    DEBUG_PRINTF("wrong resend times!!\r\n");
                    break;
                }

                DEBUG_PRINTF("state: %d, value: %d, battery: %d, rssi: %d, snr: %d, cnt: %d\r\n",
                             node_info.state,
                             node_info.value,
                             (node_info.battery * 2),
                             node_info.rssi,
                             (((int8_t)rx_pkt->payload[9] & 0x3f) - 20),
                             node_info.cnt);                    

                refresh_detector_info(node_info, i);

                break;					
            }
        }      

        /* not in list but received */
        if(i >= g_detector_info_list.num)
        {
            insert_new_device(id, NODE_DEVICE_TYPE_DETECTOR);
        }
        
        break;
    }
    case LORA_CMD_LIGHT_RESPONSE:
    case LORA_CMD_LIGHT_HEART_BEAT:
    {
        int i;
        single_light_info_t  light_info;
        
        for(i = 0; i < g_light_info_list.num; i++)
        {
            if(id == g_light_info_list.light_info[i].id)
            {
                rt_memset(&light_info, 0, sizeof(light_info));
                light_info.id            = id;
                light_info.current_color = rx_pkt->payload[5];
                light_info.rssi          = rx_pkt->payload[7];
                light_info.snr           = rx_pkt->payload[8];

                DEBUG_PRINTF("\r\n\r\n----- receive light heartbeat from %d -----\r\n", 
                            light_info.id);
                refresh_light_info(light_info, i);
            }
        }
        
        /* not in list but received */
        if(i >= g_detector_info_list.num)
        {
            insert_new_device(id, NODE_DEVICE_TYPE_LIGHT);
        }
        
        break;
    }
    case LORA_CMD_NODE_ONLINE:
    {
        int  i;
        char log_buf[128] = {0};

        for(i = 0; i < g_detector_info_list.num; i++)
        {
            if(g_detector_info_list.detector_info[i].id == id)
            {
                g_detector_info_list.detector_info[i].send_num_before_this_time +=
                     g_detector_info_list.detector_info[i].rounds * 256 + 
                     g_detector_info_list.detector_info[i].cnt - 
                     g_detector_info_list.detector_info[i].first_cnt;
                g_detector_info_list.detector_info[i].rounds = 0;
                g_detector_info_list.detector_info[i].recv_pack_flag = 0;
                g_detector_info_list.detector_info[i].reset_time++;

                work_state_log_add_event(EVENT_DETECTOR_RESTART);
                DEBUG_PRINTF("\r\n!!!!!! detector %d is restart !!!!!!\r\n", 
                             g_detector_info_list.detector_info[i].id);
                rt_snprintf(log_buf, sizeof(log_buf),
                            "%d restart",
                            g_detector_info_list.detector_info[i].id);
                add_log(log_buf);
                break;
            }
        }
        break;
    }
    case LORA_CMD_NODE_ENTER_CFG:
    {
        int i;
        char log_buf[128] = {0};
        
        for(i = 0; i < g_detector_info_list.num; i++)
        {
            if(id == g_detector_info_list.detector_info[i].id)
            {
                DEBUG_PRINTF("%d enter config\r\n", g_detector_info_list.detector_info[i].id);
                g_detector_info_list.detector_info[i].is_in_config = RT_TRUE;
                rt_snprintf(log_buf, sizeof(log_buf),
                            "%d enter config",
                            g_detector_info_list.detector_info[i].id);
                add_log(log_buf);                
                break;
            }
        }            
        break;
    }
    default:
    {
        DEBUG_PRINTF("got unknown lora cmd\r\n");
        break;
    }
    }
}

/**
 * @brief  calculate lights' free parking in g_light_info_list, if should
 *         change color, send a message to thread_lora_send to control light
 */
static void analyse_light_state(void)
{
	int i, j, k, bit;
	static int index = 0;
    bool is_first;

    /* calculate number of free parking */
	rt_memset((void*)g_relation_list.empty,
	          0, 
		      sizeof(g_relation_list.empty));
	for(i = 0; i < g_detector_info_list.num; i++)
	{
		if(g_detector_info_list.detector_info[i].state == 0)
		{
			// 
			for(j = 0; j < g_relation_list.light_num; j++)
			{
				k   = j / 8;
				bit = j % 8;
				if(g_relation_list.relation[i].mask[k] & (0x01 << bit))
				{
					g_relation_list.empty[j]++;
				}
			}
		}
	}

	/* calculate light color */
    i = index;
    is_first = true;
	while((i != index || is_first) && g_light_info_list.num > 0)
	{
        is_first = false;

        rt_mutex_take(&mutex_light_list, RT_WAITING_FOREVER);
		if(g_relation_list.empty[i] > 0)
		{
			g_light_info_list.light_info[i].correct_color = LIGHT_COLOR_GREEN;
		}
		else
		{
			g_light_info_list.light_info[i].correct_color = LIGHT_COLOR_RED;
		}
        rt_mutex_release(&mutex_light_list);

		if(g_light_info_list.light_info[i].state != NODE_STATE_OFFLINE)
		{
			if(g_light_info_list.light_info[i].correct_color !=
			   g_light_info_list.light_info[i].current_color)
			{
				if(g_light_info_list.light_info[i].time_left > 0)
				{
                    rt_uint16_t     crc_value;
                    stu_lora_msg    msg;
                    char            log_buf[128] = {'\0'};
                    rt_lora_pkt_t   tx_pkt = (rt_lora_pkt_t)msg.data;
                    
                    rt_memset(&msg, 0, sizeof(msg));

					DEBUG_PRINTF("change light %d color to %d\r\n", 
					             g_light_info_list.light_info[i].id, 
                                 g_light_info_list.light_info[i].correct_color);
                    rt_snprintf(log_buf, sizeof(log_buf),
                                "light %d color to %d",
                                g_light_info_list.light_info[i].id, 
                                g_light_info_list.light_info[i].correct_color);
                    add_log(log_buf);
                    
					// control single light at one time
                    tx_pkt->freq_hz     = get_tx_freq();
                    tx_pkt->datarate    = 9;
                    tx_pkt->len         = 12;
                    
                    tx_pkt->payload[0]  = (g_light_info_list.light_info[i].id >> 24) & 0xff;
                    tx_pkt->payload[1]  = (g_light_info_list.light_info[i].id >> 16) & 0xff;
                    tx_pkt->payload[2]  = (g_light_info_list.light_info[i].id >> 8)  & 0xff;
                    tx_pkt->payload[3]  =  g_light_info_list.light_info[i].id        & 0xff;
                    tx_pkt->payload[4]  =  LORA_CMD_CONTROL_LIGHT;
                    tx_pkt->payload[5]  =  g_light_info_list.light_info[i].correct_color;

                    crc_value           =  crc_calculate(tx_pkt->payload, 10);
                    tx_pkt->payload[10] = (crc_value >> 8) & 0xff;
                    tx_pkt->payload[11] =  crc_value       & 0xff;
                    
                    msg.type = MSG_LORA_SEND_DATA;                    
                    rt_mq_send(mq_lora_send, &msg, sizeof(msg));
                    
                    g_light_info_list.light_info[i].time_left--;
                    
					index = i + 1;
					index = (index >= g_light_info_list.num) ? 0 : index;					
					return;
				}
				else
				{
					DEBUG_PRINTF("light %d lose control\r\n",
					             g_light_info_list.light_info[i].id);
					g_light_info_list.light_info[i].state = NODE_STATE_OFFLINE;
				}
			}
		}
        
        if(++i >= g_light_info_list.num)
        {
            i = 0;
        }
	}
}

/**
 * @brief  check device belone to this concentrator LoRa connect state
 */
static void check_lora_node_connect(void)
{
	int  i;
	char log_buf[128] = {0};

	/* check detector */
	for(i = 0; i < g_detector_info_list.num; i++)
	{
		if(g_detector_info_list.detector_info[i].state != NODE_STATE_OFFLINE)
		{
			if(g_detector_info_list.detector_info[i].interval < 35)
			{
				g_detector_info_list.detector_info[i].interval++;
                set_led(ON, (i / 8), (i % 8));
			}
			else
			{
				g_detector_info_list.detector_info[i].state = NODE_STATE_OFFLINE;
                set_led(OFF, (i / 8), (i % 8));
				work_state_log_add_event(EVENT_DETECTOR_OFFLINE);
				DEBUG_PRINTF("detector %d is offline\r\n", 
				             g_detector_info_list.detector_info[i].id);
				rt_snprintf(log_buf, sizeof(log_buf),
				            "%d offline",
                            g_detector_info_list.detector_info[i].id);
				add_log(log_buf);

				g_detector_info_list.detector_info[i].offline_time++;
			}
		}
	}

	/* light */
	for(i = 0; i < g_light_info_list.num; i++)
	{
		if(g_light_info_list.light_info[i].state != NODE_STATE_OFFLINE)
		{
			if(g_light_info_list.light_info[i].interval < 35)
			{
				g_light_info_list.light_info[i].interval++;	
			}
			else
			{
				g_light_info_list.light_info[i].state = NODE_STATE_OFFLINE;
				work_state_log_add_event(EVENT_LIGHT_OFFLINE);
				DEBUG_PRINTF("light %d offline\r\n", g_light_info_list.light_info[i].id);
			}
		}
	}
}

/**
 * @brief  fill detector informations to data buffer
 * @param  data: pointer to data buffer
 * @retval data length
 */
rt_uint16_t get_self_detector_info(char *data)
{
	int         i;
	rt_uint8_t  *detector_num = (rt_uint8_t*)data;
	rt_uint32_t tmp_id;
    rt_int16_t  tmp16;

	(*detector_num) = 0;
	data++;

	for(i = 0; i < g_detector_info_list.num; i++)
	{
		(*detector_num)++;

		tmp_id = htonl(g_detector_info_list.detector_info[i].id);
		rt_memcpy(data, &tmp_id, sizeof(tmp_id));
		data    += 4;

        tmp16 = (int16_t)htons((rt_uint16_t)g_detector_info_list.detector_info[i].c_avg_rssi);
        rt_memcpy(data, &tmp16, sizeof(tmp16));
        data    += 2; 

		*data++  = g_detector_info_list.detector_info[i].c_avg_snr;
        *data++  = g_detector_info_list.detector_info[i].current_dr;
		*data++  = g_detector_info_list.detector_info[i].lowest_power;
        if(g_detector_info_list.detector_info[i].state == NODE_STATE_OFFLINE)
        {
            *data++  = g_detector_info_list.detector_info[i].state;	
        }
        else
        {
            *data++  = g_detector_info_list.detector_info[i].value;	
        }
	}
	
	return ((*detector_num) * 10 + 1);	
}

/**
 * @brief  perpare to configure lora nodes by a range of devices' id
 * @param  fd: pc connected tcp socket fd 
 * @param  data: pointer to buffer containing pc command payload
 */
static void perpare_config_node_by_range(int fd, char * data)
{
    rt_uint32_t freq;
    rt_uint16_t period;
    rt_uint32_t first_id, last_id, tmp_id;
    char        result;

    working_mode.fd      = fd;
    working_mode.pack_sn = 1;
    
    freq = *(rt_uint32_t*)data;
    freq = ntohl(freq);
    data += 4;
    if(freq < 470300000 || freq > 489300000)
    {
        result = 0;
        lora_send_tcp_data (working_mode.fd, 
                            CMD_LORA_CONFIG_NODE_BY_RANGE, 
                            &result, 
                            working_mode.pack_sn, 
                            sizeof(result));
		return;
    }

    period = *(rt_uint16_t*)data;
    period = ntohs(period);
    data += 2;
    if(period < 1 || period > 7200)
    {
        result = 0;
        lora_send_tcp_data (working_mode.fd, 
                            CMD_LORA_CONFIG_NODE_BY_RANGE, 
                            &result, 
                            working_mode.pack_sn, 
                            sizeof(result));
		return;    
    }

    first_id = *(rt_uint32_t*)data;
    first_id = ntohl(first_id);
    data += 4;

    last_id = *(rt_uint32_t*)data;
    last_id = ntohl(last_id);
    data += 4;

    rt_memset(&g_config_node_list, 0, sizeof(g_config_node_list));

    g_config_node_list.freq     = freq;
    g_config_node_list.period   = period;
    for(tmp_id = first_id; tmp_id < (last_id + 1); tmp_id++)
    {
        g_config_node_list.node[g_config_node_list.num++].id = tmp_id;

        if(g_config_node_list.num >= MAX_NODE_WHOLE_PARKING_LOT)
        {
            break;
        }
    }

    result = 1;
    lora_send_tcp_data (working_mode.fd, 
                        CMD_LORA_CONFIG_NODE_BY_RANGE, 
                        &result, 
                        working_mode.pack_sn, 
                        sizeof(result));
    
    working_mode.pack_sn++;
    working_mode.mode = MODE_CONFIG_NODE_BY_RANGE;
}

/**
 * @brief  perpare to configure lora nodes by specific devices' id
 * @param  fd: pc connected tcp socket fd 
 * @param  data: pointer to buffer containing pc command payload
 */
static void perpare_config_node_by_id(int fd, char * data)
{
    rt_uint32_t freq;
    rt_uint16_t period;
    rt_uint8_t  node_num, i;
    rt_uint32_t tmp_id;
    char        result;

    working_mode.fd      = fd;
    working_mode.pack_sn = 1;
    
    freq = *(rt_uint32_t*)data;
    freq = ntohl(freq);
    data += 4;
    if(freq < 470300000 || freq > 489300000)
    {
        result = 0;
        lora_send_tcp_data (working_mode.fd, 
                            CMD_LORA_CONFIG_NODE_BY_RANGE, 
                            &result, 
                            working_mode.pack_sn, 
                            sizeof(result));
		return;
    }

    period = *(rt_uint16_t*)data;
    period = ntohs(period);
    data += 2;
    if(period < 1 || period > 7200)
    {
        result = 0;
        lora_send_tcp_data (working_mode.fd, 
                            CMD_LORA_CONFIG_NODE_BY_RANGE, 
                            &result, 
                            working_mode.pack_sn, 
                            sizeof(result));
		return;    
    }
    
    node_num = *data++;

    rt_memset(&g_config_node_list, 0, sizeof(g_config_node_list));

    g_config_node_list.freq     = freq;
    g_config_node_list.period   = period;
    for(i = 0; i < node_num; i++)
    {
        tmp_id = *(rt_uint32_t*)data;
        tmp_id = ntohl(tmp_id);
        g_config_node_list.node[g_config_node_list.num++].id = tmp_id;
        data += 4;

        if(g_config_node_list.num >= MAX_NODE_WHOLE_PARKING_LOT)
        {
            break;
        }
    }

    result = 1;
    lora_send_tcp_data (working_mode.fd, 
                        CMD_LORA_CONFIG_NODE_BY_RANGE, 
                        &result, 
                        working_mode.pack_sn, 
                        sizeof(result));
    
    working_mode.pack_sn++;
    working_mode.mode = MODE_CONFIG_NODE_BY_ID;
}

/**
 * @brief  perpare to initailize lora nodes
 * @param  fd: pc connected tcp socket fd 
 * @param  data: pointer to buffer containing pc command payload
 */
static void perpair_init_node(int fd, char * data)
{
    rt_uint32_t first_id, last_id, tmp_id;
    char        result;

    working_mode.fd      = fd;
    working_mode.pack_sn = 1;
    
    first_id = *(rt_uint32_t*)data;
    first_id = ntohl(first_id);
    data += 4;

    last_id = *(rt_uint32_t*)data;
    last_id = ntohl(last_id);
    data += 4;

    rt_memset(&g_config_node_list, 0, sizeof(g_config_node_list));

    for(tmp_id = first_id; tmp_id < (last_id + 1); tmp_id++)
    {
        g_config_node_list.node[g_config_node_list.num++].id = tmp_id;
                
        if(g_config_node_list.num >= MAX_NODE_WHOLE_PARKING_LOT)
        {
            break;
        }
    }

    result = 1;
    lora_send_tcp_data (working_mode.fd, 
                        CMD_LORA_INIT_NODE, 
                        &result, 
                        working_mode.pack_sn, 
                        sizeof(result));
    
    working_mode.pack_sn++;
    working_mode.mode = MODE_INIT_NODE;
}

/**
 * @brief  data process thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_data_process(void* parameter)
{
    stu_lora_msg  msg;
    rt_timer_t    timer_analize_light;
    rt_timer_t    timer_device_connect;
    
    /* initailize timers */
    timer_analize_light = rt_timer_create(RT_TIMER_NAME_ANALYZE_LIGHT,
                                          callback_timer_analyze_light,
                                          RT_NULL,
                                          RT_TIMER_TIMEOUT_ANALYZE_LIGHT,
                                          RT_TIMER_FLAG_PERIODIC);
    RT_ASSERT(timer_analize_light != RT_NULL);
    rt_timer_start(timer_analize_light);
    
    timer_device_connect = rt_timer_create(RT_TIMER_NAME_DEV_CONNECT,
                                           callback_timer_dev_connect,
                                           RT_NULL,
                                           RT_TIMER_TIMEOUT_DEV_CONNECT,
                                           RT_TIMER_FLAG_PERIODIC);
    RT_ASSERT(timer_device_connect != RT_NULL);
    rt_timer_start(timer_device_connect);
    
    /* thread loop */
    while(1)
    {
        /* fetch messages */
        if(rt_mq_recv(mq_data_proc, &msg, sizeof(msg), RT_WAITING_FOREVER) == RT_EOK)
        {            
            switch(msg.type)
            {
            case MSG_LORA_RECV_DATA:
            {
                analyze_lora_pkt((rt_lora_pkt_t)msg.data);
                break;
            }
            case MSG_ANALIZE_LIGHT_STATE:
            {
                analyse_light_state();
                break;
            }
            case MSG_CHECK_DEVICE_CONNECT:
            {
                check_lora_node_connect();
                break;
            }
            case MSG_LORA_RECV_TEST:
            {
                int * tmp_fd;

                if(working_mode.mode != MODE_RECV_TEST)
                {
                    tmp_fd = (int *)msg.data;
                    working_mode.fd      = *tmp_fd;
                    working_mode.pack_sn = 1;
                    working_mode.mode    = MODE_RECV_TEST;
                }
                else
                {
                    working_mode.mode = MODE_NORMAL;
                }
                break;
            }
            case MSG_LORA_CONFIG_NODE_BY_RANGE:
            {
                int * tmp_fd;

                tmp_fd = (int *)msg.data;  
                DEBUG_PRINTF("enter config node, fd %d\r\n", *tmp_fd);
                perpare_config_node_by_range(*tmp_fd, (char *)(tmp_fd + 1));
                break;
            }
            case MSG_LORA_CONFIG_NODE_BY_ID:
            {
                int * tmp_fd;

                tmp_fd = (int *)msg.data;  
                DEBUG_PRINTF("enter config node by id, fd %d\r\n", *tmp_fd);
                perpare_config_node_by_id(*tmp_fd, (char *)(tmp_fd + 1));                
                break;
            }
            case MSG_LORA_INIT_NODE:
            {
                int * tmp_fd;

                tmp_fd = (int *)msg.data;                  
                perpair_init_node(*tmp_fd, (char *)(tmp_fd + 1));
                break;
            }
            case MSG_DATA_PROC_FEED_DOG:
            {
                data_proc_feed_dog();
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
