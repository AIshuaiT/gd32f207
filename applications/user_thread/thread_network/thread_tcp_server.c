/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_tcp_server.c
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

#include "embedded_flash.h"
#include "external_flash.h"
#include "thread_network.h"
#include "thread_sysctrl.h"
#include "thread_lora.h"
#include "thread_led.h"

#include "pcf8563.h"

#include <stdio.h>              // sscanf

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define TCP_SERVER_PORT             (5211)

/* need less than NUM_SOCKETS - 3(udp socket + tcp server socket + refuse new fd) */
#define MAX_TCP_CONNECT             RT_LWIP_TCP_PCB_NUM

#define min(a, b)     (((a) < (b)) ? (a) : (b))
#define max(a, b)     min((b), (a))

/* for debug */
#define DEBUG_TCP   1

#if DEBUG_TCP
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(...)
#endif /* DEBUG_TCP */
 
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

rt_thread_t tid_tcp_server = RT_NULL;
 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static char     tcp_buf[MAX_TCP_DATA_LENGTH];       /* tcp buffer */
static size_t   cmd_buf_len = 0;                    /* data in cmd_buf length */
static char     cmd_buf[MAX_TCP_DATA_LENGTH];       /* buffer containing completed command */
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static rt_uint8_t   set_device_id_and_mac   (rt_uint32_t id, char mac[6]);
static rt_uint8_t   set_datetime            (char *data);
static rt_uint8_t   ip_str_to_digit         (char *ip_str, rt_uint32_t *ip_value);
static rt_uint8_t   ip_digit_to_str         (rt_uint32_t ip_value, char *ip_str, char *str_len);
static rt_uint8_t   netmask_str_to_digit    (char *netmask_str, rt_uint32_t *netmask_value);
static rt_uint8_t   set_remote_server_ip    (char *data, rt_uint16_t data_len);
static rt_uint8_t   set_device_net_params   (char *data, rt_uint16_t data_len);
static rt_uint16_t  get_device_net_params   (char *data);
static rt_uint16_t  get_dev_all_info        (char * data);
static rt_uint8_t   set_lora_params         (char *data, rt_uint16_t data_len);
static rt_uint16_t  get_lora_params         (char *data);
static rt_uint8_t   set_offline_timeout     (char *data, rt_uint16_t data_len);
static rt_uint16_t  get_offline_timeout     (char *data);
static void         analyze_command         (int fd, char *data, size_t len);
static void         handle_recv_pack        (int fd, char *data, size_t len);
static int          set_socket_fd_keepalive (int fd);
 
/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern rt_uint32_t  get_ip_addr             (void);
extern rt_uint32_t  get_gw_addr             (void);
extern rt_uint32_t  get_netmask             (void);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief  set device id and ethernet mac address
 * @param  id: device id to set
 * @param  mac: array containing mac address
 * @retval 1 for success, 0 for failure
 */
static rt_uint8_t set_device_id_and_mac(rt_uint32_t id, char mac[6])
{
    device_params_t tmp_device;
    
    /* check id */
	if(((id/1000)%1000) != 448)
    {
        return 0;
    }

    /* check mac */
	if(mac[0] != 0x00 || mac[1] != 0x1e || mac[2] != 0x38)
    {
        /* not for Learn device, need 00-1e-38-xx-xx-xx */
		return 0;
    }

	tmp_device.id = id;
	rt_memcpy(tmp_device.eth_mac, mac, 6);

	return (save_device_params((rt_uint16_t*)&tmp_device) == RT_EOK) ? 1 : 0;
}

/**
 * @brief  set machine datetime
 * @param  data: pointer to command buffer
 * @retval 1 for success, 0 for failed
 */
static rt_uint8_t set_datetime(char *data)
{
	RTC_T datetime;

	rt_uint16_t tmp_year = *(rt_uint16_t*)data;
	
	datetime.year  = ntohs(tmp_year);
	data += 2;
	datetime.month  = *data++;
	datetime.day    = *data++;
	datetime.hour   = *data++;
	datetime.minute	= *data++;
	datetime.second = *data;
	//DEBUG_PRINTF("year:%d,month:%d,day:%d,hour:%d,min:%d,sen:%d\n",datetime.year,
	//		datetime.month,datetime.day,datetime.hour,datetime.minute,datetime.second);
	return (pcf8563_set_datetime(&datetime) == RT_EOK) ? 1 : 0;

}

/**
 * @brief  transform ip address from string to hex
 * @param  ip_str: pointer to ip string
 * @param  ip_value: pointer to output hex variable
 * @retval 1 for success, 0 for failed
 */
static rt_uint8_t ip_str_to_digit(char *ip_str, rt_uint32_t *ip_value)
{
	int ip[4];
	
	if(sscanf(ip_str, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) != 4)
    {
		return 0;
    }
										  
	if(!((ip[0] >= 0 && ip[0] <= 255) &&
         (ip[1] >= 0 && ip[1] <= 255) &&
	     (ip[2] >= 0 && ip[2] <= 255) &&
	     (ip[3] >= 0 && ip[3] <= 255)))
    {
		return 0;
    }
	
	*ip_value = (rt_uint32_t)((ip[3] << 24) |
                           (ip[2] << 16) |
                           (ip[1] << 8 ) |
                           (ip[0]      )); 

	return 1;
}

/**
 * @brief  transform netmask from string to hex
 * @param  netmask_str: pointer to netmask string
 * @param  netmask_value: pointer to output hex variable
 * @retval 1 for success, 0 for failed
 */
static rt_uint8_t netmask_str_to_digit(char *netmask_str,
                                    rt_uint32_t *netmask_value)
{
	rt_uint32_t tmp_value;

	if(!ip_str_to_digit(netmask_str, netmask_value))
    {
	    return 0;
    }

	/* transform to little-endian */
	tmp_value = ntohl(*netmask_value);
    
    /* check validity */
	if(((tmp_value - 1) | tmp_value) == 0xffffffff)
	{
	    return 1;
	}
	else
    {
		return 0;
    }
}

/**
 * @brief  transform ip address from hex to string
 * @param  ip_str: pointer to ip string
 * @param  ip_value: pointer to output hex variable
 * @retval 1 for success, 0 for failed
 */
static rt_uint8_t ip_digit_to_str(rt_uint32_t ip_value,
                               char *ip_str,
							   char *str_len)
{	
	rt_uint32_t    ip[4];
	char        tmp_str[16];
	int         i;

	if(ip_str == NULL || str_len == NULL)
    {
		return 0;
    }

	for(i = 0; i < 4; i++)
	{
		ip[i] = (ip_value >> (8 * i)) & 0xff;
	}

	rt_sprintf(tmp_str, "%d.%d.%d.%d", 
	        ip[0], ip[1], ip[2], ip[3]);

	*str_len = strlen(tmp_str);
	rt_memcpy(ip_str, tmp_str, *str_len);

	return 1;
}

/**
 * @brief  set remote host ip address for cross-network
 * @param  data: pointer to command buffer
 * @param  data_len: command data length
 * @retval 1 for success, 0 for failed
 */
static rt_uint8_t set_remote_server_ip(char *data, rt_uint16_t data_len)
{
	int      port;
	char     pc_ip_len, hvcs_ip_len;
	char     pc_ip[16] = {0}, hvcs_ip[16] = {0};
	rt_uint32_t tmp_pc_ip, tmp_hvcs_ip;

	port = (int)(*(int*)data);
	port = ntohl(port);
	data += 4;

	if(port != REMOTE_UDP_SERVER_PORT)
    {
		return 0;
    }

	pc_ip_len    = *data++;
	rt_memcpy(pc_ip, data, pc_ip_len);
	data        += pc_ip_len;
	hvcs_ip_len  = *data++;
	rt_memcpy(hvcs_ip, data, hvcs_ip_len);

	/* data not correct */
	if(data_len != pc_ip_len + hvcs_ip_len + 6)
    {
        return 0;
    }

	/* check params */
	if(!ip_str_to_digit(pc_ip, &tmp_pc_ip))
    {
	    return 0;
    }
	if(!ip_str_to_digit(hvcs_ip, &tmp_hvcs_ip))
    {
	    return 0;
    }

	g_wnc_config_bak.net_config.pc_ip   = tmp_pc_ip;
	g_wnc_config_bak.net_config.hvcs_ip = tmp_hvcs_ip;
    set_wnc_config();

	return 1;
}

/**
 * @brief  set device network parameters
 * @param  data: pointer to command buffer
 * @param  data_len : command length
 * @2retval 1 for success, 0 for failed
 */
static rt_uint8_t set_device_net_params(char *data, rt_uint16_t data_len)
{
	char net_mode;
	char ip_len, netmask_len, gateway_len;
	char ip[16] = {0}, netmask[16] = {0}, gateway[16] = {0};
	rt_uint32_t tmp_ip, tmp_netmask, tmp_gateway;

	data++;
	net_mode     = *data++;	

    if(net_mode == 0)
	{
		g_wnc_config_bak.net_config.net_mode = DHCP_MODE;		
		set_wnc_config();
	    return 1;
	}
    else
    {
        ip_len       = *data++;
        rt_memcpy(ip, data, ip_len);
        data        += ip_len;
        netmask_len  = *data++;
        rt_memcpy(netmask, data, netmask_len);
        data        += netmask_len;
        gateway_len  = *data++;
        rt_memcpy(gateway, data, gateway_len);

        /* data not correct */
        if(data_len != ip_len + netmask_len + gateway_len + 5)
        {
            return 0;
        }

        /* check and transform params */
        if(!ip_str_to_digit(ip, &tmp_ip))
        {
            return 0;
        }
        if(!ip_str_to_digit(gateway, &tmp_gateway))
        {
            return 0;
        }
        if(!netmask_str_to_digit(netmask, &tmp_netmask))
        {
            return 0;
        }

        g_wnc_config_bak.net_config.net_mode = DEDICATED_MODE;
        g_wnc_config_bak.net_config.ip       = tmp_ip;
        g_wnc_config_bak.net_config.gateway  = tmp_gateway;
        g_wnc_config_bak.net_config.netmask  = tmp_netmask;
        set_wnc_config();

        return 1;
    }
}

/**
 * @brief  get device network parameters
 * @param  data: pointer to data buffer
 * @retval data length
 */
static rt_uint16_t get_device_net_params(char *data)
{
	char        *ip_len, *netmask_len, *gateway_len;
	char        *ip,     *netmask,     *gateway;
    rt_uint32_t    ip_addr, netmask_addr, gateway_addr;
    
    extern int is_dhcp_enable;
    
    /* network mode */
	*data++ = (!is_dhcp_enable);

	ip_len  = data++;
	ip      = data;
    ip_addr = get_ip_addr();
	ip_digit_to_str(ip_addr, ip, ip_len);
	data += *ip_len;

	netmask_len  = data++;
	netmask      = data;
    netmask_addr = get_netmask();
	ip_digit_to_str(netmask_addr, netmask, netmask_len);
	data += *netmask_len;

	gateway_len  = data++;
	gateway      = data;
    gateway_addr = get_gw_addr();
	ip_digit_to_str(gateway_addr, gateway, gateway_len);

    /* 1 byte network mode, 3 bytes length */
	return (*ip_len + *netmask_len + *gateway_len + 4);
}

/**
 * @brief  package all device informations as user protocol into data buffer
 * @param  data: pointer to data buffer
 */
static rt_uint16_t get_dev_all_info(char * data)
{
    rt_uint16_t data_len;
    rt_uint32_t tmp32;
    
    /* network parameters */
    data_len  = get_device_net_params(data);			
    data     += data_len;

    /* configure file serial number */
    tmp32 = get_file_sn();
    tmp32 = htonl(tmp32);
    rt_memcpy(data, &tmp32, sizeof(tmp32));
    data     += sizeof(tmp32);
    data_len += sizeof(tmp32); 

    /* reserved */
    *data++ = 0;
    data_len++;

    /* reserved */
    *data++ = 0;
    data_len++;

    /* reserved */
    *data++ = 0;
    data_len++;

    /* lora parameters */
    tmp32 = htonl(g_wnc_config.lora_config.rf_freq);
    rt_memcpy(data, &tmp32, sizeof(tmp32));
    data     += sizeof(tmp32);
    data_len += sizeof(tmp32);

    *data++ = g_wnc_config.lora_config.rf_power;
    data_len++;

    /* firmware version */
    rt_memcpy(data, SOFTWARE_VERSION, 10);
    data     += 10;
    data_len += 10;

    /* PC server ip */
    tmp32 = g_wnc_config.net_config.pc_ip;
    rt_memcpy(data, &tmp32, 4);
    data     += 6;
    data_len += 6;

    /* hvcs ip */
    tmp32 = g_wnc_config.net_config.hvcs_ip;
    rt_memcpy(data, &tmp32, 4);
    data     += 6;
    data_len += 6;    
    
    return data_len;
}

/**
 * @brief  set lora parameters
 * @param  data: pointer to command buffer
 * @param  data_len: command length
 * @retval 1 for success, 0 for failed
 */
static rt_uint8_t set_lora_params(char *data, rt_uint16_t data_len)
{
	rt_uint32_t tmp_freq;
	rt_uint8_t  tmp_power;

	if(data_len != 6)
    {
		return 0;
    }

	tmp_freq  = (((rt_uint32_t)data[1] << 24) & 0xff000000) |
	            (((rt_uint32_t)data[2] << 16) & 0x00ff0000) |
				(((rt_uint32_t)data[3] << 8 ) & 0x0000ff00) |
				(((rt_uint32_t)data[4]        & 0x000000ff));
	tmp_power = data[5];

	/* valid is 470.3 ~ 489.3MHz */
	if(tmp_freq < 470300000 || tmp_freq > 489300000)
    {
		return 0;
    }

	/* valid is 5~17dBm */
	if(tmp_power < 5 || tmp_power > 17)
    {
		return 0;
    }

	g_wnc_config_bak.lora_config.rf_freq  = tmp_freq;
	g_wnc_config_bak.lora_config.rf_power = tmp_power;
    set_wnc_config();

	return 1;
}

/**
 * @brief  get lora parameters
 * @param  data: pointer to data buffer
 * @retval data length
 */
static rt_uint16_t get_lora_params(char *data)
{
    rt_uint32_t tmp_freq;
    
    tmp_freq = htonl(g_wnc_config.lora_config.rf_freq);
    rt_memcpy(data, &tmp_freq, sizeof(tmp_freq));
    data += sizeof(tmp_freq);
    
    *data = g_wnc_config.lora_config.rf_power;
    
    return ((rt_uint16_t)5); /* always 5 */
}

/**
 * @brief  set ditector offline timeout
 * @param  data: pointer to command buffer
 * @param  data_len: command length
 * @retval 1 for success, 0 for falied
 *
 * @NOTE   this time depend on detector report period, now need a little bit
 *         longer than 2 multiple period.
 */
static rt_uint8_t set_offline_timeout(char *data, rt_uint16_t data_len)
{
	rt_uint16_t tmp_time;

	if(data_len != 3)
    {
        return 0;
    }

    data++;
	tmp_time = (*(rt_uint16_t*)data);
    tmp_time = ntohs(tmp_time);
    
    /* never longer than 4 hours now */
	if(tmp_time > 245)
    {
        return 0;
    }

	g_wnc_config_bak.lora_config.time_offline = tmp_time;
	set_wnc_config();

	return 1;
}

/**
 * @brief  get ditector offline timeout
 * @param  data: pointer to data buffer
 * @retval data length
 */
static rt_uint16_t get_offline_timeout(char *data)
{
    data[0] = (g_wnc_config.lora_config.time_offline >> 8 ) & 0xff;
    data[1] =  g_wnc_config.lora_config.time_offline        & 0xff;
    
    return ((rt_uint16_t)2); /* always 2 */
}

/**
 * @brief  analyze tcp commands
 * @param  fd: socket fd
 * @param  data: pointer to command buffer
 * @param  len: command length 
 */
static void analyze_command(int fd, char *data, size_t len)
{
    tcp_pack_header_t   header;
    char                *payload;
    rt_uint16_t         data_len;
    
    if(xor_verify(data, (len - 1)) != data[len - 1])
    {
        /* checksum error */
        goto tcp_bad_cmd;
    }
    
    header = (tcp_pack_header_t)data;
    
    if((header->id != htonl(wnc_device.id)) &&
       (header->id != 0) && 
       (header->cmd != CMD_SET_DEV_ID_AND_MAC))
    {
        /* command not for this device */
        goto tcp_bad_cmd;
    }

    data_len = ntohs(header->data_len);
    payload  = data + sizeof(struct tcp_pack_header);
        
	switch(header->cmd)
	{
    case CMD_SET_DEV_ID_AND_MAC:
    {
        *payload = set_device_id_and_mac(ntohl(header->id), payload);
        data_len = 1;
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    case CMD_ERASE_DEV_ID_AND_MAC:
    {
        *payload = (init_device_param() == RT_EOK);
        data_len = 1;
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    case CMD_SOFT_REBOOT:
    {
        stu_sysctrl_msg msg;
        
        /* send message to system control thread */
        rt_memset(&msg, 0, sizeof(msg));        
        msg.type = MSG_SYS_USER_REBOOT;        
        rt_mq_send(mq_sys_ctrl, &msg, sizeof(msg));
        
        *payload = 1;   /* always success */
        data_len = 1;
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    case CMD_ADJUST_MACHINE_CLOCK:
    {
        rt_uint8_t ret;
        
        ret = set_datetime(payload);
        
        *payload = ret;
        
        /* datalen not changed */
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);       
        break;
    }
    case CMD_READ_MACHINE_CLOCK:
    {
        data_len = 7;
        memset(payload, 0, data_len);
        get_datetime(payload);        
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    case CMD_SET_SERVER_IP:
    {
        *payload = set_remote_server_ip(payload, data_len);
        /* datalen not changed */
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);          
        break;
    }
    case CMD_CONFIGURE_FILES_OPT:								//下载、上传配置文件
    {
        if((header->pack_sn == 1) && (*payload == 0))
        {
            /* upload */
            int16_t ret;
            rt_uint16_t max_size;
            
            max_size = MAX_TCP_DATA_LENGTH - sizeof(struct tcp_pack_header) - 1;
            
            while((ret = upload_file(header->pack_sn, payload, max_size)) > 0)
            {
                data_len = (rt_uint16_t)ret;
                header->data_len = htons(data_len);
                len = sizeof(struct tcp_pack_header) + data_len + 1;
                *(data + len - 1) = xor_verify(data, (len - 1));                
                send(fd, data, len, 0); 
                header->pack_sn++;
                
                /**
                 * sleep for a while to do other threads works, upload
                 * may take a long time
                 */
                rt_thread_delay(5);
            }
            
            file_operate_reset();
            
            /* upload failed */
            if(ret == -1)
            {
                DEBUG_PRINTF("upload failed\r\n");
                *payload = 0;
                data_len = 1;
                header->data_len = htons(data_len);
                len = sizeof(struct tcp_pack_header) + data_len + 1;
                *(data + len - 1) = xor_verify(data, (len - 1));
                send(fd, data, len, 0);                
            }
        }
        else
        {
            rt_uint8_t ret;
            
            ret = download_file(header->pack_sn, payload, data_len);
            
            /* download */
            if(ret == DOWNLOAD_DOWNLOADING)
            {
                /* downloading, do nothing */
            }
            else
            {
                *payload = ret;   /* use result directly */
                header->pack_sn = 1;
                data_len = 1;
                header->data_len = htons(data_len);
                len = sizeof(struct tcp_pack_header) + data_len + 1;
                *(data + len - 1) = xor_verify(data, (len - 1));
                send(fd, data, len, 0);                
            }
        }
        break;
    }
    case CMD_NET_PARAMS_OPT:
    {
        rt_uint8_t operate = *payload;
        
        if(operate == 1 /* set */)
        {
            rt_uint8_t ret;
            
            ret = set_device_net_params(payload, data_len);
            
            *payload = ret;
            *(data + len - 1) = xor_verify(data, (len - 1));
            send(fd, data, len, 0);            
        }
        else  /* get */
        {
            *payload = 1;   /* always success */
            data_len = 1;
            data_len += get_device_net_params(payload + 1);
            header->data_len = htons(data_len);
            len = sizeof(struct tcp_pack_header) + data_len + 1;
            *(data + len - 1) = xor_verify(data, (len - 1));
            send(fd, data, len, 0);             
        }
        break;
    }
    case CMD_GET_DEVICE_ALL_INFOS:
    {
        /* always success */
        data_len = get_dev_all_info(payload);
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    case CMD_GET_SLOT_INFOS:
    {
        data_len = get_self_detector_info(payload);
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);        
        break;
    }
    case CMD_LORA_SEND_TEST:			//LORA无线发送测试
    {
        /* to lora send thread */
        {
            stu_lora_msg msg;
            
            msg.type = MSG_LORA_SEND_TEST;        
            rt_mq_send(mq_lora_send, &msg, sizeof(msg));
        }
        
        /* to led thread */
        {
            stu_led_msg msg;

            msg.type = MSG_LED_TEST;        
            rt_mq_send(mq_led, &msg, sizeof(msg));
        }
        
        *payload = 1;   /* always success */
        data_len = 1;
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    case CMD_LORA_RECV_TEST:		//无线接收测试
    {
        /* to lora receive thread */
        {
            stu_lora_msg    msg;
            int *           p_data;
            
            p_data = (int *)msg.data;
            
            /* send message to lora receive thread */
            rt_memset(&msg, 0, sizeof(msg));        
            msg.type = MSG_LORA_RECV_TEST;
            *p_data = fd;
            rt_mq_send(mq_data_proc, &msg, sizeof(msg));
        }
        
        /* to led thread */
        {
            stu_led_msg msg;

            msg.type = MSG_LED_TEST;        
            rt_mq_send(mq_led, &msg, sizeof(msg));
        }
        
        break;
    }
    case CMD_USER_INIT:
    {
        user_init();
        *payload = 1;   /* always success */
        data_len = 1;
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);        
        break;
    }
    case CMD_LOCAL_LORA_PARAMS_OPT:
    {
        rt_uint8_t operate = *payload;
        
        if(operate == 1 /* set */)
        {
            rt_uint8_t ret;
            
            ret = set_lora_params(payload, data_len);
            
            *payload = ret;
            *(data + len - 1) = xor_verify(data, (len - 1));
            send(fd, data, len, 0);             
        }
        else  /* get */
        {
            data_len = get_lora_params(payload);
            header->data_len = htons(data_len);
            len = sizeof(struct tcp_pack_header) + data_len + 1;
            *(data + len - 1) = xor_verify(data, (len - 1));
            send(fd, data, len, 0);
        }
            
        break;
    }
    case CMD_SENSOR_OFFLINE_TIMEOUT:
    {
        rt_uint8_t operate = *payload;
        
        if(operate == 1 /* set */)
        {
            rt_uint8_t ret;
            
            ret = set_offline_timeout(payload, data_len);
            
            *payload = ret;
            data_len = 1;
            header->data_len = htons(data_len);
            len = sizeof(struct tcp_pack_header) + data_len + 1;
            *(data + len - 1) = xor_verify(data, (len - 1));
            send(fd, data, len, 0);             
        }
        else  /* get */
        {
            data_len = get_offline_timeout(payload);
            header->data_len = htons(data_len);
            len = sizeof(struct tcp_pack_header) + data_len + 1;
            *(data + len - 1) = xor_verify(data, (len - 1));
            send(fd, data, len, 0);            
        }
        break;
        
    }
    case CMD_LORA_CONFIG_NODE_BY_RANGE:
    {
        stu_lora_msg    msg;
        int *           p_data;
        
        p_data = (int *)msg.data;
        
        /* send message to system control thread */
        rt_memset(&msg, 0, sizeof(msg));        
        msg.type = MSG_LORA_CONFIG_NODE_BY_RANGE;
        *p_data++ = fd;
        rt_memcpy(p_data, payload, data_len);
        rt_mq_send(mq_data_proc, &msg, sizeof(msg));
        
        break;
    }
    case CMD_LORA_INIT_NODE:
    {
        stu_lora_msg    msg;
        int *           p_data;
        
        p_data = (int *)msg.data;
        
        /* send message to system control thread */
        rt_memset(&msg, 0, sizeof(msg));        
        msg.type = MSG_LORA_INIT_NODE;
        *p_data++ = fd;
        rt_memcpy(p_data, payload, data_len);
        rt_mq_send(mq_data_proc, &msg, sizeof(msg));        
        break;
    }
    case CMD_LORA_CONFIG_NODE_BY_ID:
    {
        stu_lora_msg    msg;
        int *           p_data;
        
        DEBUG_PRINTF("in tcp, config by id\r\n");
        
        p_data = (int *)msg.data;
        
        /* send message to system control thread */
        rt_memset(&msg, 0, sizeof(msg));        
        msg.type = MSG_LORA_CONFIG_NODE_BY_ID;
        *p_data++ = fd;
        rt_memcpy(p_data, payload, data_len);
        rt_mq_send(mq_data_proc, &msg, sizeof(msg));        
        break;
    }
    default:
tcp_bad_cmd:
    {
        header->id = htonl(wnc_device.id);
        rt_memcpy(header->device_type, DEVICE_TYPE, strlen(DEVICE_TYPE));
        header->pack_sn = 1;
        data_len = 0;
        header->data_len = htons(data_len);
        len = sizeof(struct tcp_pack_header) + data_len + 1;
        *(data + len - 1) = xor_verify(data, (len - 1));
        send(fd, data, len, 0);
        break;
    }
    }
}

/**
 * @brief  a packet use user protocol may be divided by tcp low level,
 *         check if we received a completed packet and combine them together again
 * @brief  fd: socket fd
 * @param  data: pointer to recv data buf.
 * @param  len: data length
 */
static void handle_recv_pack(int fd, char *data, size_t len)
{
    size_t              copy_len;               /* data length for once copy */
    static size_t       pack_len;               /* completed packet length */
    
    tcp_pack_header_t   header;
    static rt_uint8_t   new_pack = 1;
    
    while(len > 0)
    {
        /* new packet need calculate packet length again */
        if(new_pack)
        {
            header = (tcp_pack_header_t)data;
            if(!rt_strncmp(header->device_type, DEVICE_TYPE, rt_strlen(DEVICE_TYPE)))
            {
                new_pack = 0;            
                pack_len = sizeof(struct tcp_pack_header) + ntohs(header->data_len) + 1;
                
                /* reset command buffer */
                cmd_buf_len = 0;
                rt_memset(cmd_buf, 0, sizeof(cmd_buf));
            }
            else
            {
                data++;
                len--;
                continue;
            }
        }

        copy_len = min((MAX_TCP_DATA_LENGTH - cmd_buf_len), len);
        copy_len = min(copy_len, (pack_len - cmd_buf_len));
        
        /* copy data to command buffer */
        rt_memcpy(&cmd_buf[cmd_buf_len], data, copy_len);
        cmd_buf_len += copy_len;
        data        += copy_len;
        len         -= copy_len;
        
        if(cmd_buf_len == pack_len)
        {
            /* get a completed command */
            analyze_command(fd, cmd_buf, pack_len);       

            /* followed data should be new packet */
            new_pack = 1;
        }
    }
}

/**
 * @brief  set socket fd keepalive option
 * @param  fd: socket fd need set keepalive
 */
static int set_socket_fd_keepalive(int fd)
{
	int ret = 0;
	int keepalive = 1; 		// keepalive open
	int keepidle = 10; 		// check connect when 10s idle
	int keepinterval = 5; 	// every 5s detect when check connect
	int keepcount = 3; 		// maximum check 3 times

	ret |= setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive , sizeof(keepalive));
	ret |= setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&keepidle , sizeof(keepidle));
	ret |= setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepinterval , sizeof(keepinterval));
	ret |= setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepcount , sizeof(keepcount));

	return ret;
}

/**
 * @brief  calculate xor verify
 * @param  data: pointer to data buffer
 * @param  len:  data length
 * @retval xor result
 */
char xor_verify(const char *data, int len)
{
	int i;
	char result = 0;

	if(data == RT_NULL) return 0;

  	for(i = 0; i < len; i++)
	{
		result ^= data[i];
	}

	return result;
}

/**
 * @brief  read datetime to data
 * @param  data: pointer to buffer
 */
void get_datetime(char *data)
{
	RTC_T datetime;

	if(pcf8563_get_datetime(&datetime) != RT_EOK)
    {
        return;
    }

	*data++ = (char)(datetime.year >> 8);
	*data++ = (char)(datetime.year & 0xff);
	*data++ = datetime.month;
	*data++ = datetime.day;
	*data++ = datetime.hour;
	*data++ = datetime.minute;
	*data++ = datetime.second;
}

/**
 * @brief  tcp server thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_tcp_server(void* parameter)
{
    int server_fd, client_fd[MAX_TCP_CONNECT], max_fd, new_fd;
    int connect_num;
    struct sockaddr_in server_addr;
    fd_set rdst;
    int i, j;
    int ret;
    
    int32_t recv_len;
    
    /* create socket */
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        // need send to sysctrl
    }
    
    /* set socket unblock */
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    
    /* initailize tcp server address */
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(TCP_SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    rt_memset(&(server_addr.sin_zero), 8, sizeof(server_addr.sin_zero));

    /* bind socket */
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
    {
        // need send to sysctrl
    }
    
    if(listen(server_fd, MAX_TCP_CONNECT) == -1)
    {
        // need send to sysctrl
    }
    
    rt_memset(client_fd, 0, sizeof(client_fd));
    connect_num = 0;

    while(1)
    {
        FD_ZERO(&rdst);
        FD_SET(server_fd, &rdst);
        
        /* find maximum fd */
        max_fd = server_fd;        
        for(i = 0; i < connect_num; i++)
        {
            FD_SET(client_fd[i], &rdst);
            max_fd = (client_fd[i] > max_fd) ? client_fd[i] : max_fd;
        }

        /* wait forever until have data or accept */
        ret = select(max_fd + 1, &rdst, RT_NULL, RT_NULL, RT_NULL);
        
        /* select error */
        if(ret < 0) continue;
        
        /* check connects */
        for(i = 0; i < connect_num; i++)
        {
            if(FD_ISSET(client_fd[i], &rdst))
            {
                rt_memset(tcp_buf, 0, sizeof(tcp_buf));
                recv_len = recv(client_fd[i], tcp_buf, MAX_TCP_DATA_LENGTH, 0);
                if(recv_len <= 0)
                {
                    DEBUG_PRINTF("client %d disconnect\r\n", client_fd[i]);
                    /* disconnect */
                    close(client_fd[i]);
                    FD_CLR(client_fd[i], &rdst);
                    client_fd[i] = 0;
                }
                else
                {
                    handle_recv_pack(client_fd[i], tcp_buf, (size_t)recv_len);
                }
            }
        }
        
        /* refresh connect list */
        for(i = 0, j = 0; i < connect_num; i++)
        {
            if(client_fd[i] != 0)
            {
                client_fd[j++] = client_fd[i];
            }            
        }
        connect_num = j;
        
        /* check new accept */
        if(FD_ISSET(server_fd, &rdst))
        {
            if((new_fd = accept(server_fd, RT_NULL, RT_NULL)) < 0)
            {
                DEBUG_PRINTF("new fd < 0\r\n");
                continue;
            }
            
            /* set keepalive */
            if(set_socket_fd_keepalive(new_fd) != 0)
            {
                DEBUG_PRINTF("new fd set keepalive failed\r\n");
                continue;
            }
            
            /* add new client */
            if(connect_num < MAX_TCP_CONNECT)
            {
                DEBUG_PRINTF("new clinet connect %d\r\n", new_fd);
                client_fd[connect_num] = new_fd;
                FD_SET(client_fd[connect_num], &rdst);
                connect_num++;
            }
            else
            {
                DEBUG_PRINTF("over max connect, refuse\r\n");
                send(new_fd, " bye", 4, 0);
                close(new_fd);
            }
        }
    }
}
 
/* ****************************** end of file ****************************** */
