/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_udp.c
 * Arthor    : Test
 * Date      : May 4th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-04      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include <lwip/sockets.h>

#include "thread_network.h"
#include "thread_sysctrl.h"

#include "embedded_flash.h"
#include "external_flash.h"
#include "wnc_data_base.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define MAX_NODE_NUM_IN_UDP             (80)
#define UDP_SEND_PERIOD                 (10 * RT_TICK_PER_SECOND)       /* 10s */

#define LOCAL_UDP_SERVER_PORT           (5212)
#define REMOTE_UDP_BROADCAST_ADDR       (0xffffffff)

#define PC_RESP_STRING                  "PCNC"

/* timers */
#define RT_TIMER_NAME_UDP_ALL           "udp_send_all"
#define RT_TIMER_TIMEOUT_UDP_ALL        (15 * 60 * RT_TICK_PER_SECOND)  /* 15min */

/* for debug */
#define DEBUG_UDP   0

#if DEBUG_UDP
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(...)
#endif /* DEBUG_UDP */
 
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

rt_thread_t tid_udp_client = RT_NULL;
rt_thread_t tid_udp_server = RT_NULL;
 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static int  send_all  = 0;
static int  socket_fd = -1;         /* udp socket handler */

static char send_buf[1024];         /* udp send buffer */
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static size_t fill_udp_buffer(char *data_buf);

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
static void callback_timer_udp(void* parameter)
{
    send_all = 1;
}

/**
 * @brief  restore informations want to send through udp to data_buf
 * @param  data_buf: pointer to data buffer use for send udp 
 */
static size_t fill_udp_buffer(char *data_buf)
{
    int         i;
    size_t      len;
    rt_uint32_t tmp32;
    char        *buf;
    char        tmp_str[20];
    char        *lora_node_num;
    
    buf = data_buf;
    
    /* device id */
    tmp32 = htonl(wnc_device.id);
    rt_memcpy(buf, &tmp32, 4);
    buf += 4;
    
    /* device type */
    rt_memcpy(buf, "LN", 4);
    buf += 4;
    
    /* device id string */
    rt_sprintf(tmp_str, "%d", wnc_device.id);
    rt_memcpy(buf, tmp_str, 10);
    buf += 20;

    /* device ethernet MAC */
    rt_memcpy(buf, wnc_device.eth_mac, 6);
    buf += 6;
    
    /* software version */
    rt_memcpy(buf, SOFTWARE_VERSION, 10);
    buf += 10;

    /* hardware version */
    rt_memcpy(buf, HARDWARE_VERSION, 10);
    buf += 10;
    
    /* datatime */
    get_datetime(buf);
    buf += 7;
    
    /* PC server IP address */
    tmp32 = g_wnc_config.net_config.pc_ip;
    rt_memcpy(buf, &tmp32, 4);
    buf += 6;
    
    /* HVCS IP address */
    tmp32 = g_wnc_config.net_config.hvcs_ip;
    rt_memcpy(buf, &tmp32, 4);
    buf += 6;
    
    /* configuration files serial number */
    tmp32 = get_file_sn();
    tmp32 = htonl(tmp32);
    rt_memcpy(buf, &tmp32, 4);
    buf += 4;
    
    /* reserved */
    *buf++ = 0;
    
    /* lora node device number */
    lora_node_num = buf++;
    
    /* one lora node device information length */
    *buf++ = 10;
    
    /* lora node device informations */
	for(i = 0; i < g_detector_info_list.num; i++)
	{
		if((*lora_node_num) >= MAX_NODE_NUM_IN_UDP)
        {
			break;
        }

		if(send_all || g_detector_info_list.detector_info[i].is_park_state_changed)
		{
			(*lora_node_num)++;
			*buf++  = NODE_DEVICE_TYPE_DETECTOR;
			tmp32 = g_detector_info_list.detector_info[i].id;
			tmp32 = htonl(tmp32);
			rt_memcpy(buf, &tmp32, sizeof(tmp32));
			buf    += 4;
			*buf++  = g_detector_info_list.detector_info[i].state;

			*buf++  = g_detector_info_list.detector_info[i].rssi;
			*buf++  = g_detector_info_list.detector_info[i].snr;
            *buf++  = g_detector_info_list.detector_info[i].current_dr;
			*buf++  = g_detector_info_list.detector_info[i].battery;
			g_detector_info_list.detector_info[i].is_park_state_changed = 0;	
		}
	}

	for(i = 0; i < g_light_info_list.num; i++)
	{
		if((*lora_node_num) >= MAX_NODE_NUM_IN_UDP)
        {
			break;
        }
        
		(*lora_node_num)++;
		*buf++  = NODE_DEVICE_TYPE_LIGHT;	 /* ???????? */
		tmp32 = g_light_info_list.light_info[i].id;
		tmp32 = htonl(tmp32);
		rt_memcpy(buf, &tmp32, sizeof(tmp32));
		buf    += 4;
		*buf++  = (g_light_info_list.light_info[i].state == NODE_STATE_OFFLINE) ? 
		            NODE_STATE_OFFLINE : g_light_info_list.light_info[i].current_color;
		*buf++  = g_light_info_list.light_info[i].rssi;
		*buf    = g_light_info_list.light_info[i].current_color;
		buf    += 3;		
	}

	for(i = 0; i < g_new_node_list.num; i++)
	{
		if((*lora_node_num) >= MAX_NODE_NUM_IN_UDP)
        {
			break;
        }

		if(g_new_node_list.node[i].time_left)
		{
			(*lora_node_num)++;
			g_new_node_list.node[i].time_left--;
			*buf++  = g_new_node_list.node[i].device_type;
			tmp32 = g_new_node_list.node[i].id;
			tmp32 = htonl(tmp32);
			memcpy(buf, &tmp32, sizeof(tmp32));
            buf    += 4;
            *buf++  = NODE_STATE_NEW_DEVICE;			
			*buf++  = g_new_node_list.node[i].rssi;
			buf    += 3;
		}
	}    
    
    send_all = 0;
    
    len = buf - data_buf;
    *buf = xor_verify(data_buf, len);
    
    return (len + 1);
}

/**
 * @brief  initailize udp socket handler
 * @retval 0 for success, -1 for failure
 */
int init_udp_socket_handler(void)
{
    struct sockaddr_in local_addr;
    
    /* create socket */
    if((socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        return -1;
    }
    
    /* initailize local address */
    local_addr.sin_family       = AF_INET;
    local_addr.sin_port         = htons(LOCAL_UDP_SERVER_PORT);
    local_addr.sin_addr.s_addr  = INADDR_ANY;
    rt_memset(&(local_addr.sin_zero), 0, sizeof(local_addr.sin_zero));
    
    /* bind socket */
    if(bind(socket_fd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) == -1)
    {
        return -1;
    }
    
    return 0;
}

/**
 * @brief  udp client thread entry.
 * @param  parameter: rt-thread param.
 */ 
void thread_udp_client(void* parameter)
{
    struct sockaddr_in  remote_addr;
    size_t              data_len;
    
    rt_timer_t          timer_udp_all;
    
    timer_udp_all = rt_timer_create(RT_TIMER_NAME_UDP_ALL,
                                    callback_timer_udp,
                                    RT_NULL,
                                    RT_TIMER_TIMEOUT_UDP_ALL,
                                    RT_TIMER_FLAG_PERIODIC);
    RT_ASSERT(timer_udp_all != RT_NULL);
    rt_timer_start(timer_udp_all);    

    /* initailize remote address */
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port   = htons(REMOTE_UDP_SERVER_PORT);    
    rt_memset(&(remote_addr.sin_zero), 0, sizeof(remote_addr.sin_zero));
    
    while(1)
    {
        rt_memset(send_buf, 0, sizeof(send_buf));
        data_len = fill_udp_buffer(send_buf);
        
        remote_addr.sin_addr.s_addr = REMOTE_UDP_BROADCAST_ADDR;
        sendto(socket_fd, send_buf, data_len, 0, 
            (struct sockaddr *)&remote_addr, sizeof(struct sockaddr));
        
        /* crose network */
        if(g_wnc_config.net_config.pc_ip != 0)
        {
            remote_addr.sin_addr.s_addr = g_wnc_config.net_config.pc_ip;
            sendto(socket_fd, send_buf, data_len, 0, 
                (struct sockaddr *)&remote_addr, sizeof(struct sockaddr));            
        }
        if(g_wnc_config.net_config.hvcs_ip != 0)
        {
            remote_addr.sin_addr.s_addr = g_wnc_config.net_config.hvcs_ip;
            sendto(socket_fd, send_buf, data_len, 0, 
                (struct sockaddr *)&remote_addr, sizeof(struct sockaddr));            
        }
        DEBUG_PRINTF("UDP borad one time\n");
        rt_thread_delay(UDP_SEND_PERIOD);
        udp_clint_feed_dog();
    }
}

/**
 * @brief  udp server thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_udp_server(void* parameter)
{
    char recv_buf[256];

    while(1)
    {
        rt_memset(recv_buf, 0, sizeof(recv_buf));
        recvfrom(socket_fd, recv_buf, sizeof(recv_buf), 0, RT_NULL, RT_NULL);
        
        if(rt_memcmp(&recv_buf[4], PC_RESP_STRING, strlen(PC_RESP_STRING)) == 0)
        {
            stu_sysctrl_msg msg;
            
            if(mq_sys_ctrl != RT_NULL)
            {
                msg.type = MSG_SYS_UDP_CONFIRM;        
                rt_mq_send(mq_sys_ctrl, &msg, sizeof(msg));
            }
                
            //receive pc response
            DEBUG_PRINTF("recv pc resp\r\n");
        }
    }
}

 
/* ****************************** end of file ****************************** */
