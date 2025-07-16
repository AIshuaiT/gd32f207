/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : work_state_log.c
 * Arthor    : Test
 * Date      : May 19th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-19      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "log.h"
#include "pcf8563.h"
#include "embedded_flash.h"
#include "external_flash.h"
#include "wnc_data_base.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

enum log_reason
{
    REASON_NOT_WRITE = 0,
    REASON_RESEND,
    REASON_SEND,
    REASON_MISS_FRAME,
    REASON_OFFLINE,
    REASON_RESET,
    REASON_CHANGE,
    REASON_POWER,
    REASON_LORA,    
};
 
/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containg work state during current time interval
 */
struct stu_work_state
{
	RTC_T    time_start;
	int      events[MAX_EVENT_TYPE]; 
};

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

static struct stu_work_state g_work_state;
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static rt_uint8_t   need_write_to_log   (int index);
 
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
 * @brief  check if the detector informations need to write to log this time
 * @param  index: index of detector in g_detector_info_list
 */
static rt_uint8_t need_write_to_log(int index)
{
    rt_uint8_t  tmp_ptr;
    rt_uint32_t resend;

    resend = (g_detector_info_list.detector_info[index].resend1 +
              g_detector_info_list.detector_info[index].resend2 * 2 + 
              g_detector_info_list.detector_info[index].resend3 * 3) - 
             (g_detector_info_list.detector_info[index].last_resend1 +
              g_detector_info_list.detector_info[index].last_resend2 * 2 + 
              g_detector_info_list.detector_info[index].last_resend3 * 3);
    if(resend > 10)
    {
        return REASON_RESEND;
    }

    if((g_detector_info_list.detector_info[index].send_num - 
        g_detector_info_list.detector_info[index].last_send) > 12)
    {
        return REASON_SEND;
    }

    if(g_detector_info_list.detector_info[index].last_miss_recv != 
       g_detector_info_list.detector_info[index].miss_recv_num)
    {
        return REASON_MISS_FRAME;
    }

    if((g_detector_info_list.detector_info[index].state == NODE_STATE_OFFLINE) ||
       (g_detector_info_list.detector_info[index].last_offline != 
        g_detector_info_list.detector_info[index].offline_time))
    {
        return REASON_OFFLINE;
    }

    if(g_detector_info_list.detector_info[index].last_offline != 
       g_detector_info_list.detector_info[index].offline_time)
    {
        return REASON_RESET;
    }

    if(g_detector_info_list.detector_info[index].change_time > 10)
    {
        return REASON_CHANGE;
    }

    if((g_detector_info_list.detector_info[index].lowest_power < 140) &&
       (g_detector_info_list.detector_info[index].lowest_power != 0))
    {
        return REASON_POWER;
    }

    tmp_ptr = (g_detector_info_list.detector_info[index].c_ptr == 0) ?
               MAX_AVG_NUM : (g_detector_info_list.detector_info[index].c_ptr - 1);
    if((g_detector_info_list.detector_info[index].current_dr >= 9) ||
       (g_detector_info_list.detector_info[index].c_rssi[tmp_ptr] < -100) ||
       (g_detector_info_list.detector_info[index].c_snr[tmp_ptr] < -5))
    {
        return REASON_LORA;
    }

    return REASON_NOT_WRITE;
}

/**
 * @brief  get online detectors number
 * @retval online detectors number
 */
static rt_uint32_t get_online_detector(void)
{
	int i;
	rt_uint32_t retval = 0;
	
	for(i = 0; i < g_detector_info_list.num; i++)
	{
		if(g_detector_info_list.detector_info[i].state != NODE_STATE_OFFLINE)
        {
			retval++;
        }
	}
	
	return retval;	
}

/**
 * @brief  get online lights number
 * @retval online lights number
 */
static rt_uint32_t get_online_light(void)
{
	int i;
	rt_uint32_t retval = 0;
	
	for(i = 0; i < g_light_info_list.num; i++)
	{
		if(g_light_info_list.light_info[i].state != NODE_STATE_OFFLINE)
        {
			retval++;
        }
	}
	
	return retval;	
}
 
 /**
 * @brief  perpare to read work state log
 * @retval file size
 */
rt_size_t prepare_read_work_state_log(void)
{
    rt_size_t size;
    
    prepare_read(WORK_STATE_LOG);
    size = calculate_file_size(WORK_STATE_LOG);
    
    return size;
}

/**
 * @brief  read work state log data to buffer
 * @param  buf: pointer to data buffer
 * @param  max_size: data size want to read
 * @retval size read out
 */
rt_size_t read_work_state_log(char * buf, rt_uint32_t max_size)
{
    return log_read(buf, max_size, WORK_STATE_LOG);
}

/**
 * @brief  write power on informations to log
 */
void work_state_power_on(void)
{
    RTC_T       rtc;
    rt_uint32_t len;
    char        str_log[256];
    
    pcf8563_get_datetime(&rtc);
    
	rt_memset(&g_work_state, 0, sizeof(g_work_state));
	g_work_state.time_start = rtc;
    
	/* power on time */
	len = rt_snprintf(str_log, sizeof(str_log),
                "\r\n-----------\r\n%d/%d/%d  %02d:%02d  system start\r\n",
		        rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute);			
	log_write(str_log, len, WORK_STATE_LOG); 

	/* versions */
	len = rt_snprintf(str_log, sizeof(str_log), 
                "hardware version:%s\r\n",
				HARDWARE_VERSION);
    log_write(str_log, len, WORK_STATE_LOG);

	len = rt_snprintf(str_log, sizeof(str_log), 
                "software version:%s\r\n",
			 	SOFTWARE_VERSION);
    log_write(str_log, len, WORK_STATE_LOG);

	/* id */
	len = rt_snprintf(str_log, sizeof(str_log), 
                "device id: %d\r\n", 
                wnc_device.id);
    log_write(str_log, len, WORK_STATE_LOG);

	/* network */
    len = rt_snprintf(str_log, sizeof(str_log), 
                "MAC: %02x-%02x-%02x-%02x-%02x-%02x\r\n", 
                wnc_device.eth_mac[0], 
                wnc_device.eth_mac[1], 
                wnc_device.eth_mac[2], 
                wnc_device.eth_mac[3], 
                wnc_device.eth_mac[4], 
                wnc_device.eth_mac[5]);
    log_write(str_log, len, WORK_STATE_LOG);

	len = rt_snprintf(str_log, sizeof(str_log), 
                "network mode: %s\r\n", 
                ((g_wnc_config.net_config.net_mode == DHCP_MODE) ? "DHCP" : "dedicated IP"));
	log_write(str_log, len, WORK_STATE_LOG);

	/* configure info */
	len = rt_snprintf(str_log, sizeof(str_log), 
                    "file serial number: %d\r\n", 
                    get_file_sn());
    log_write(str_log, len, WORK_STATE_LOG);
    len = rt_snprintf(str_log, sizeof(str_log), 
                    "detector number: %d\r\n", 
                    g_detector_info_list.num);
    log_write(str_log, len, WORK_STATE_LOG);

    len = rt_snprintf(str_log, sizeof(str_log), 
                    "light number: %d\r\n", 
                    g_light_info_list.num);
    log_write(str_log, len, WORK_STATE_LOG);    
}

/**
 * @brief  write work state during this time period to log
 */
void work_state_time_period(void)
{
   	rt_int32_t  len, j;
    rt_tick_t   now_tick;
    rt_uint32_t tmp_num;
    RTC_T rtc;

    rt_uint8_t  tmp_ptr;
    rt_uint8_t  ret;
    char        str_log[256];
    
    rt_uint32_t tmp_ip;
    extern rt_uint32_t get_ip_addr(void);

	rt_memset(&rtc, 0, sizeof(rtc));
    pcf8563_get_datetime(&rtc);

    /* time */
    len = rt_snprintf(str_log, sizeof(str_log), 
                    "\r\n-----------\r\n%02d/%02d %02d:%02d -- %02d/%02d %02d:%02d\r\n",
                    g_work_state.time_start.month, 
                    g_work_state.time_start.day, 
                    g_work_state.time_start.hour, 
                    g_work_state.time_start.minute, 
                    rtc.month, 
                    rtc.day, 
                    rtc.hour, 
                    rtc.minute);
	log_write(str_log, len, WORK_STATE_LOG);

	now_tick = rt_tick_get() / RT_TICK_PER_SECOND;    
    len = rt_snprintf(str_log, sizeof(str_log), 
                    "continuous work: %d days %d hours %d minitues\r\n",
                    now_tick/3600/24,
                    (now_tick%(3600*24))/3600,
                    (now_tick%3600)/60);
    log_write(str_log, len, WORK_STATE_LOG);

    /* device informations */
    len = rt_snprintf(str_log, sizeof(str_log), 
                "software version:%s\r\n",
			 	SOFTWARE_VERSION);
    log_write(str_log, len, WORK_STATE_LOG);
    
    tmp_ip = get_ip_addr();
    len = rt_snprintf(str_log, sizeof(str_log), 
                    "ip: %d.%d.%d.%d\r\n",
                    ((tmp_ip)       & 0xff),
                    ((tmp_ip >>  8) & 0xff),
                    ((tmp_ip >> 16) & 0xff),
                    ((tmp_ip >> 24) & 0xff));
    log_write(str_log, len, WORK_STATE_LOG);
    
    /* node informations */
	len = rt_snprintf(str_log, sizeof(str_log), 
                    "detector num: %d, ",
                    g_detector_info_list.num);
    log_write(str_log, len, WORK_STATE_LOG);

	tmp_num = get_online_detector();
	len = rt_snprintf(str_log, sizeof(str_log), "online: %d\r\n", tmp_num);
    log_write(str_log, len, WORK_STATE_LOG);

	len = rt_snprintf(str_log, sizeof(str_log), 
                    "light num: %d, ",
                    g_light_info_list.num);
    log_write(str_log, len, WORK_STATE_LOG);

	tmp_num = get_online_light();
	len = rt_snprintf(str_log, sizeof(str_log), "online: %d\r\n", tmp_num);
    log_write(str_log, len, WORK_STATE_LOG);

    /* errors */
	log_write("errors:\r\n", rt_strlen("errors:\r\n"), WORK_STATE_LOG);

	len = rt_snprintf(str_log, sizeof(str_log), 
                    "detector offline times:%d\r\n",
                    g_work_state.events[EVENT_DETECTOR_OFFLINE]);
    log_write(str_log, len, WORK_STATE_LOG);
	len = rt_snprintf(str_log, sizeof(str_log), 
                    "detector reset times:%d\r\n",
                    g_work_state.events[EVENT_DETECTOR_RESTART]);
    log_write(str_log, len, WORK_STATE_LOG);

	/* state for each detector */
	log_write("detector state:\r\n", rt_strlen("detector state:\r\n"), WORK_STATE_LOG);

	len = rt_snprintf(str_log, sizeof(str_log),
                    "    ID     | state |  send  |  recv  |  resp  |"
                    " rs1 | rs2 | rs3 | chg | off | rst | pwr | SF | rssi | snr | reason\r\n");
	log_write(str_log, len, WORK_STATE_LOG); 

	for(j = 0; j < g_detector_info_list.num; j++) 
    {
        ret = need_write_to_log(j);
        if(ret != REASON_NOT_WRITE)
        {
            tmp_ptr = (g_detector_info_list.detector_info[j].c_ptr == 0) ?
                       MAX_AVG_NUM : (g_detector_info_list.detector_info[j].c_ptr - 1);
    		len = rt_snprintf(str_log, sizeof(str_log),
                            "%10d | %5d | %6d | %6d | %6d | %3d | %3d | %3d | %3d | %3d | %3d | %3d | %2d | %4d | %3d | ",
                            g_detector_info_list.detector_info[j].id,
                            g_detector_info_list.detector_info[j].state,
                            g_detector_info_list.detector_info[j].send_num,
                            g_detector_info_list.detector_info[j].abs_recv_num,
                            g_detector_info_list.detector_info[j].recv_num,
                            g_detector_info_list.detector_info[j].resend1,
                            g_detector_info_list.detector_info[j].resend2,
                            g_detector_info_list.detector_info[j].resend3,
                            g_detector_info_list.detector_info[j].change_time,
                            g_detector_info_list.detector_info[j].offline_time,
                            g_detector_info_list.detector_info[j].reset_time,
                            (g_detector_info_list.detector_info[j].lowest_power * 2),
                            g_detector_info_list.detector_info[j].current_dr,
                            g_detector_info_list.detector_info[j].c_rssi[tmp_ptr],
                            g_detector_info_list.detector_info[j].c_snr[tmp_ptr]);
    		log_write(str_log, len, WORK_STATE_LOG);

            switch(ret)
            {
            case REASON_RESEND:
                len = rt_snprintf(str_log, sizeof(str_log), "resend\r\n");
                break;
            case REASON_SEND:
                len = rt_snprintf(str_log, sizeof(str_log), "send\r\n");
                break;
            case REASON_MISS_FRAME:
                len = rt_snprintf(str_log, sizeof(str_log), "miss frame\r\n");
                break;
            case REASON_OFFLINE:
                len = rt_snprintf(str_log, sizeof(str_log), "offline\r\n");
                break;
            case REASON_RESET:
                len = rt_snprintf(str_log, sizeof(str_log), "reset\r\n");
                break;
            case REASON_CHANGE:
                len = rt_snprintf(str_log, sizeof(str_log), "change\r\n");
                break;
            case REASON_POWER:
                len = rt_snprintf(str_log, sizeof(str_log), "power\r\n");
                break;
            case REASON_LORA:
                len = rt_snprintf(str_log, sizeof(str_log), "lora\r\n");
                break;
            default:
                len = rt_snprintf(str_log, sizeof(str_log), "by mistake\r\n"); 
                break;
            }
            log_write(str_log, len, WORK_STATE_LOG);
        }

        g_detector_info_list.detector_info[j].last_resend1   = g_detector_info_list.detector_info[j].resend1;
        g_detector_info_list.detector_info[j].last_resend2   = g_detector_info_list.detector_info[j].resend2;
        g_detector_info_list.detector_info[j].last_resend3   = g_detector_info_list.detector_info[j].resend3;
        g_detector_info_list.detector_info[j].last_offline   = g_detector_info_list.detector_info[j].offline_time;
        g_detector_info_list.detector_info[j].last_reset     = g_detector_info_list.detector_info[j].reset_time;
        g_detector_info_list.detector_info[j].last_send      = g_detector_info_list.detector_info[j].send_num;
        g_detector_info_list.detector_info[j].last_miss_recv = g_detector_info_list.detector_info[j].miss_recv_num;
        g_detector_info_list.detector_info[j].change_time    = 0; 
	}

    rt_memset(&g_work_state.events, 0, sizeof(g_work_state.events));
	g_work_state.time_start = rtc;
}

rt_int32_t work_state_log_add_event(enum log_event_type event)    
{
    if(event >= MAX_EVENT_TYPE)
    {
        return -1;
    }
    
    g_work_state.events[event]++;
    return 0;
}

/* ****************************** end of file ****************************** */
