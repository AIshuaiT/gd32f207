/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : external_flash.c
 * Arthor    : Test
 * Date      : May 3rd, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-03      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */
 
#include <rtthread.h>
#include "external_flash.h"
#include "embedded_flash.h"
#include "wnc_data_base.h"
#include "log.h"

#include <lwip/def.h>
#include <stdio.h>

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define CONFIG_VERSION              (0x20170503)        /* change when structure wnc_cfg_t is changed */

#define DEF_RF_FREQ                 (471100000)         /* in Hz */
#define DEF_RF_POWER                (17)                /* in dBm */
#define DEF_LORA_TIME_OFFLINE       (35)                /* in minitue */

#define DEF_NET_MODE                (0)
#define DEF_DEV_IP                  (0x0200a8c0)        /* 192.168.0.2 */
#define DEF_GATEWAY                 (0x0100aec0)        /* 192.168.0.1 */
#define DEF_NETMASK                 (0x00ffffff)        /* 255.255.255.0 */
#define DEF_PC_IP                   (0)                 /* not cross network */
#define DEF_HVCS_IP                 (0)

#define FILE_OPT_STATE_IDLE         (0x00)
#define FILE_OPT_STATE_DOWNLOAD     (0x01)
#define FILE_OPT_STATE_UPLOAD       (0x02)

#define MAX_FILE_NAME_LEN           (256)

/* for debug */
#define DEBUG_EX_FLASH   0

#if DEBUG_EX_FLASH
    #define DEBUG_PRINTF    rt_kprintf
#else
    #define DEBUG_PRINTF(...)
#endif /* DEBUG_EX_FLASH */

/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containing current file operate state
 */
struct stu_file_operate_state
{
	rt_uint8_t     state;
	rt_uint8_t     file_type;
	rt_uint32_t    file_size;
	rt_uint8_t     packet_seq;
	rt_uint32_t    remain;
	rt_uint32_t    flash_addr;
	rt_uint32_t    version;
};

/**
 * @brief  structure containing single file informations
 */
__packed typedef struct
{
	rt_uint32_t    sn;
	rt_uint32_t    size;
	char        file_name[MAX_FILE_NAME_LEN];
} single_file_info_t;

/**
 * @brief  structure containing all configure file informations
 */
__packed typedef struct
{	
	single_file_info_t  region_config;  /* LN_region_config_#sn.ini */	
	single_file_info_t  light_connect;  /* LN_light_connect_#sn.ini */	
	single_file_info_t  light_parking;  /* LN_light_parking_#sn.ini */	
	single_file_info_t  sensor_list;    /* LN_sensor_#sn.ini */
	single_file_info_t  reserved_parking;/*LN_reserved_parking_#sn.ini */
} file_info_list_t;

/**
 * @brief  structure containing firmware upgrade informations
 *
 * @NOTE   this structure must same as BOOT program
 */
__packed typedef struct
{  	
	rt_uint32_t    have_firm;           /* notify whether there is firmware save in external flash */
	rt_uint32_t    need_up;	            /* notify whether the firmware need upgrade */
	rt_uint8_t     trycnt;              /* use in BOOT program */
	rt_uint32_t    success;             /* notify the firmware upgrade success */
	rt_uint8_t     ver[3];              /* already not used, reserved */
	rt_uint32_t    bin_len;             /* firmware file size */
	rt_uint8_t     verify;              /* xor verify */
} FIRMUP_FLASHINFO;

/**
 * @brief  structure containing backup firmware informations
 *
 * @NOTE   this structure must same as BOOT program
 */
__packed typedef struct
{  	
	rt_uint32_t  useful;               /* notify whether there is firmware save in external flash */
	rt_uint8_t   ver[3];               /* already not used, reserved */
	rt_uint32_t  bin_len;              /* firmware file size */
} FIRMBACK_FLASHINFO;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

wnc_cfg_t       g_wnc_config;
wnc_cfg_t       g_wnc_config_bak;

rt_device_t     dev_ext_flash;      /* only extern for log */

const char file_name_def[MAX_FILE_TYPE_NUM][24]=
{
	{"LN_region_config_"},
	{"LN_light_connect_"},
	{"LN_light_parking_"},
	{"LN_sensor_"},    
    {"LN_upgrade_"},
	{"LN.log"},
	{"LN_work_state.log"},
	{"LN_reserved_parking_"},
};

/**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

/**
 * @NOTE  device could only operate one file at one time, either upload or download
 */
static struct stu_file_operate_state    file_operate_state;

static file_info_list_t                 g_file_info_list;
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static rt_int8_t    check_wnc_config        (void);
static void         init_wnc_config         (void);
static rt_uint32_t  flash_file_read_line    (char *buf, rt_uint32_t addr, rt_uint32_t offset, int size);
static void         read_light_connect_file (void);
static void         set_region_mask_by_index(int detector_index, int region_index);
static rt_uint8_t   get_file_type           (const char * name);
static rt_uint8_t   process_download_type   (char *file_name, rt_uint32_t size);
 
/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern char     xor_verify              (const char *data, int len);
extern void     feed_dog                (void);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief  check validity of settings in g_wnc_config
 * @retval 0 for settings is valid, -1 for settings is invalid
 */
static rt_int8_t check_wnc_config(void)
{
    /* check structure version */
    if(g_wnc_config.version != CONFIG_VERSION)
    {
        return -1;
    }
    
    /* check network configuration */
    if(g_wnc_config.net_config.net_mode == DEDICATED_MODE)
    {
        if(g_wnc_config.net_config.ip == 0 ||
            g_wnc_config.net_config.ip == 0xffffffff)
        {
            return -1;
        }
        if(g_wnc_config.net_config.gateway == 0 ||
            g_wnc_config.net_config.gateway == 0xffffffff)
        {
            return -1;
        }
        if(g_wnc_config.net_config.netmask == 0 ||
            g_wnc_config.net_config.netmask == 0xffffffff)
        {
            return -1;
        }
    }
    
    /* check lora configuration */
    if(g_wnc_config.lora_config.rf_freq < 470300000 ||
        g_wnc_config.lora_config.rf_freq > 489300000)
    {
        return -1;
    }
    if(g_wnc_config.lora_config.rf_power > 17 ||
        g_wnc_config.lora_config.rf_power < 5)
    {
        return -1;
    }
    
    return 0;
}

/**
 * @brief  initailize g_wnc_config to default settings
 */
static void init_wnc_config(void)
{
    g_wnc_config.version = CONFIG_VERSION;
    g_wnc_config.lora_config.rf_freq = DEF_RF_FREQ;
    g_wnc_config.lora_config.rf_power = DEF_RF_POWER;
    g_wnc_config.lora_config.time_offline = DEF_LORA_TIME_OFFLINE;
    
    user_init();
}

/**
 * @brief  read data from external flash to data buffer until macth '\n'
 *         or reach (size - 1), last byte will write in '\0'
 * @param  buf: output data buffer
 * @param  addr: file start address on external flash
 * @param  offset: read offset from addr
 * @param  size: data buffer size
 * @retval number of bytes read out
 *
 * @NOTE   use similar as fgets() in Linux
 */
static rt_uint32_t flash_file_read_line(char *buf, rt_uint32_t addr, rt_uint32_t offset, int size)
{
	rt_uint32_t cnt = 0;
	char tmp_buf[256];
	int real_size = ((size - 1) < 256) ? (size - 1) : 256;

	if(buf == RT_NULL) 
    {
        return 0;
    }
	
    rt_device_read(dev_ext_flash, (addr + offset), tmp_buf, real_size);

	while(real_size-- > 0)
	{
		if((*buf++ = tmp_buf[cnt++]) == '\n')
        {
		    break;
        }
	}
	*buf = '\0';

	return cnt;
}

/**
 * @brief  read wireless lights belong to this concentrator
 */
static void read_light_connect_file(void)
{
	char tmp_buf[64];
	rt_uint32_t byte_left, read_len, want_len;
    rt_uint32_t offset;
    rt_uint32_t cnt;
	rt_uint32_t light_id, wnc_id;

	rt_memset(&g_relation_list,   0, sizeof(g_relation_list));
	rt_memset(&g_light_info_list, 0, sizeof(g_light_info_list));

	if((g_file_info_list.light_connect.sn == 0) ||
       (g_file_info_list.light_connect.sn == 0xffffffff))
    {
        /* file not exist */
		return;
    }

	byte_left = g_file_info_list.light_connect.size;
    cnt       = 0;
    offset    = 0;
	while((byte_left > 0) && (cnt < MAX_LIGHT_PER_WNC))
	{
        want_len = (byte_left < 64) ? (byte_left + 1) : 64;
        read_len = flash_file_read_line(tmp_buf,
                                        LIGHT_CONNECT_FILE_ADDR,
                                        offset,
                                        want_len);

		offset    += read_len;
		byte_left -= read_len;

		if((sscanf(tmp_buf, "%d;%d", (int*)&light_id, (int*)&wnc_id)) == 2)
        {
            if(wnc_id == wnc_device.id)
            {
                g_relation_list.light_id[cnt]               = light_id;
                g_light_info_list.light_info[cnt].id        = light_id;
                g_light_info_list.light_info[cnt].time_left = 10;
                cnt++;
            }
        }
	}
	g_relation_list.light_num  = cnt;
	g_light_info_list.num      = cnt;
}

/**
 * @brief  read detectors belong to this concentrator
 */
void read_sensor_list_flie(void)
{
	char tmp_buf[64];
	rt_uint32_t byte_left, read_len, want_len;
    rt_uint32_t offset;
    rt_uint32_t cnt;
	rt_uint32_t detector_id, wnc_id;

	rt_memset(&g_detector_info_list,    0, sizeof(g_detector_info_list));
    g_relation_list.detector_num = 0;
	rt_memset(g_relation_list.relation, 0, sizeof(g_relation_list.relation));

	if((g_file_info_list.sensor_list.sn == 0) ||
	   (g_file_info_list.sensor_list.sn == 0xffffffff))
    {
        /* file not exist */
		return;
    }

	byte_left = g_file_info_list.sensor_list.size;
    cnt       = 0;
    offset    = 0;    
	while((byte_left > 0) && (cnt < MAX_DETECTOR_PER_WNC))
	{
        want_len = (byte_left < 64) ? (byte_left + 1) : 64;
        read_len = flash_file_read_line(tmp_buf,
                                        SENSOR_LIST_FILE_ADDR,
                                        offset,
                                        want_len);
        
		offset    += read_len;
		byte_left -= read_len;

		if((sscanf(tmp_buf, "%d;%d", (int*)&detector_id, (int*)&wnc_id)) == 2)
        {
            if(wnc_id == wnc_device.id)
            {
                g_detector_info_list.detector_info[cnt].id    = detector_id;
                g_detector_info_list.detector_info[cnt].state = NODE_STATE_OFFLINE;
                g_relation_list.relation[cnt].detector_id     = detector_id;
                cnt++;
            }
        }
	}
	g_detector_info_list.num     = cnt;
	g_relation_list.detector_num = cnt;
}

/**
 * @brief  read devices group informations belong to this concentrator
 *         (e.g. lights and detectors)
 */
void read_light_parking_flie(void)
{
	int i, j;  /* use for loop */
	char tmp_buf[64];
	rt_uint32_t byte_left, read_len, want_len;
    rt_uint32_t offset;    
	rt_uint32_t light_id, detector_id;

	if((g_file_info_list.light_parking.sn == 0) ||
	   (g_file_info_list.light_parking.sn == 0xffffffff))
    {
        /* file not exist */
		return;
    }

	byte_left = g_file_info_list.light_parking.size;
    offset    = 0;
	while(byte_left > 0)
	{
        want_len = (byte_left < 64) ? (byte_left + 1) : 64;
        read_len = flash_file_read_line(tmp_buf,
                                        LIGHT_PARKING_FILE_ADDR,
                                        offset,
                                        want_len);        

		offset    += read_len;
		byte_left -= read_len;

		if((sscanf(tmp_buf, "%d;%d", (int*)&light_id, (int*)&detector_id)) == 2)
        {
            for(i = 0; i < g_relation_list.light_num; i++)
            {
                if(light_id == g_relation_list.light_id[i])
                {
                    for(j = 0; j < g_relation_list.detector_num; j++)
                    {
                        if(detector_id == g_relation_list.relation[j].detector_id)
                        {
                            set_region_mask_by_index(j/* detector index */,
                                                     i/* light index */);
                            break;
                        }
                    }
                    break;
                }
            }
        }
	}
}

/**
 * @brief  set device relationships in g_relation_list
 * @param  detector_index: index of detector
 * @param  region_index: index of group
 *
 * @NOTE   in array region_mask[], every bit express a group, now maximun 40 groups
 *         region_mask[0][0:7] : group[0:7]
 *         region_mask[1][0:7] : group[8:15]
 *         region_mask[2][0:7] : group[16:23]
 *         region_mask[3][0:7] : group[24:31]
 *         region_mask[4][0:7] : group[32:39]
 */
static void set_region_mask_by_index(int detector_index, int region_index)
{
	int i, bit;

	i   = region_index/8;
	bit = region_index%8;

	g_relation_list.relation[detector_index].mask[i] |= (0x01 << bit);
}

/**
 * @brief  find now is operating which file
 * @param  name: pointer to file name string
 * @retval index of file_name_def[]
 */
static rt_uint8_t get_file_type(const char * name)
{
    rt_uint8_t i;
    
    for(i = 0; i < MAX_FILE_TYPE_NUM; i++)
	{
	 	if(!rt_memcmp(name, file_name_def[i], rt_strlen(file_name_def[i])))
		{
            /* matched file type */
            return i;	
		}
	}
    
    /* not found type, i == MAX_FILE_TYPE_NUM */
    return i;
}

/**
 * @brief  process download file type
 * @param  file_name: pointer to file name string
 * @param  size: current downloading file size
 * @retval return 0 when got an error, 1 when success
 */
static rt_uint8_t process_download_type(char *file_name, rt_uint32_t size)
{
	int i;
	char *p = RT_NULL;
    
    if(file_name == RT_NULL)
    {
        return 0;
    }
    
    /* get file type */
    file_operate_state.file_type = get_file_type(file_name);
	if(file_operate_state.file_type == MAX_FILE_TYPE_NUM)
	{
        /* file type not support */
	  	return 0;
	}

    /* get file version */
    p = file_name + rt_strlen(file_name_def[file_operate_state.file_type]);
	if(sscanf(p, "%d%*s", (int *)&file_operate_state.version) != 1)
    {
        /* no version means file not correct */
		return 0;
    }

    /* pre-operate before download */
	switch(file_operate_state.file_type)
	{
	case RESERVED_PARKING:
		if(size > MAX_CONFIG_FILE_SIZE)
        {
			return 0;
        }
		g_file_info_list.reserved_parking.size = size;
		rt_strncpy(g_file_info_list.reserved_parking.file_name, file_name, 256);
		g_file_info_list.reserved_parking.file_name[255] = '\0';

		file_operate_state.flash_addr = RESERVED_PARKING_FILE_ADDR;
		file_operate_state.remain = file_operate_state.file_size = size;

		for(i = 0; i < CONFIG_FILE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((RESERVED_PARKING_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));
		}
		break;
		
	case REGION_CONFIG_FILE:
		if(size > MAX_CONFIG_FILE_SIZE)
        {
			return 0;
        }
					
		g_file_info_list.region_config.size = size;
		rt_strncpy(g_file_info_list.region_config.file_name, file_name, 256);
		g_file_info_list.region_config.file_name[255] = '\0';

		file_operate_state.flash_addr = REGION_CONFIG_FILE_ADDR;
		file_operate_state.remain = file_operate_state.file_size = size;

		for(i = 0; i < CONFIG_FILE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((REGION_CONFIG_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));
		}
		break;
	case LIGHT_CONNECT_FILE:
		if(size > MAX_CONFIG_FILE_SIZE)
        {
			return 0;
        }
					
		g_file_info_list.light_connect.size = size;
		rt_strncpy(g_file_info_list.light_connect.file_name, file_name, 256);
		g_file_info_list.light_connect.file_name[255] = '\0';

		file_operate_state.flash_addr = LIGHT_CONNECT_FILE_ADDR;
		file_operate_state.remain = file_operate_state.file_size = size;

		for(i = 0; i < CONFIG_FILE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((LIGHT_CONNECT_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));
		}
		break;
	case LIGHT_PARKING_FILE:
		if(size > MAX_CONFIG_FILE_SIZE)
        {
			return 0;
        }
					
		g_file_info_list.light_parking.size = size;
		rt_strncpy(g_file_info_list.light_parking.file_name, file_name, 256);
		g_file_info_list.light_parking.file_name[255] = '\0';

		file_operate_state.flash_addr = LIGHT_PARKING_FILE_ADDR;
		file_operate_state.remain = file_operate_state.file_size = size;

		for(i = 0; i < CONFIG_FILE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((LIGHT_PARKING_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));            
		}
		break;
	case SENSOR_LIST_FILE:
		if(size > MAX_CONFIG_FILE_SIZE)
        {
			return 0;
        }

		g_file_info_list.sensor_list.size = size;
		rt_strncpy(g_file_info_list.sensor_list.file_name, file_name, 256);
		g_file_info_list.sensor_list.file_name[255] = '\0';

		file_operate_state.flash_addr = SENSOR_LIST_FILE_ADDR;
		file_operate_state.remain = file_operate_state.file_size = size;

		for(i = 0; i < CONFIG_FILE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((SENSOR_LIST_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));             
		}
		break;
	case UPGRADE_BIN_FILE:
		if(size > (FIRMWAREUPGRADE_SCT_NUM * FLASH_BYTES_PER_SECTOR))
        {
			return 0;
        }

		file_operate_state.flash_addr = FIRMWAREUPGRADE_BASE_SCT * FLASH_BYTES_PER_SECTOR;
		file_operate_state.remain = file_operate_state.file_size = size;

		for(i = 0; i < FIRMWAREUPGRADE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((FIRMWAREUPGRADE_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));            
		}                 
		return 1;
	default:
		/* should not get here */
		return 0;
	}	

	return 1;
}

/**
 * @brief  run when successful downloaded a file
 */
static void finish_download_option(void)
{
	switch(file_operate_state.file_type)
	{
	case REGION_CONFIG_FILE:
		/* not support yet */
		return;
	case UPGRADE_BIN_FILE:
		{
			FIRMUP_FLASHINFO FirmUpFlashInfo;
			char   log_buf[256];

			memset(&FirmUpFlashInfo, 0xFF, sizeof(FirmUpFlashInfo));
			FirmUpFlashInfo.have_firm = 0xa5a5a5a5;
			FirmUpFlashInfo.need_up   = 0xa5a5a5a5;
			FirmUpFlashInfo.bin_len   = file_operate_state.file_size;
			FirmUpFlashInfo.verify	  = xor_verify((char*)&FirmUpFlashInfo, 
			                                       sizeof(FirmUpFlashInfo) - 1);
            
            rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)FIRMUPINFO_BASE_ADDR);
            rt_device_write(dev_ext_flash, FIRMUPINFO_BASE_ADDR, &FirmUpFlashInfo, sizeof(FirmUpFlashInfo));

			rt_snprintf(log_buf, sizeof(log_buf),
                        "upgrade software, version is %d",
                        file_operate_state.version);
			add_log(log_buf);
		}
		return;
	case LIGHT_CONNECT_FILE:
		g_file_info_list.light_connect.sn = file_operate_state.version;
		break;		
	case LIGHT_PARKING_FILE:
		g_file_info_list.light_parking.sn = file_operate_state.version;
		break;
	case SENSOR_LIST_FILE:
		g_file_info_list.sensor_list.sn   = file_operate_state.version;
		break; 
	default:
		/* should not get here */
		return;
	}

    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)CONFIG_FILE_INFO_ADDR);
    rt_device_write(dev_ext_flash, CONFIG_FILE_INFO_ADDR, &g_file_info_list, sizeof(g_file_info_list));

    init_parking_lists();
}

/**
 * @brief  read current operating file data from external flash
 * @param  max_size: maximum size could read
 * @retval size read out, unit byte
 */
static rt_size_t read_file(char * buf, rt_uint32_t max_size)
{
    rt_size_t size;
    
    switch(file_operate_state.file_type)
    {
    case REGION_CONFIG_FILE:
    case LIGHT_CONNECT_FILE:
    case LIGHT_PARKING_FILE:
    case SENSOR_LIST_FILE:
    {
        size = rt_device_read(dev_ext_flash, file_operate_state.flash_addr, buf, max_size);
        break;
    }
    case SYSTEM_LOG_FILE:
    {
        size = read_system_log(buf, max_size);
        break;
    }
    case WORKSTATE_LOG_FILE:
    {
        size = read_work_state_log(buf, max_size);
        break;
    }
    default:
    {
        /* should never get here */
        size = 0;
        break;
    }
    }
    
    return size;
}

/**
 * @brief  initailize part of g_wnc_config to default settings
 */
void user_init(void)
{
    g_wnc_config.net_config.net_mode = DEF_NET_MODE;
    g_wnc_config.net_config.ip       = DEF_DEV_IP;
    g_wnc_config.net_config.gateway  = DEF_GATEWAY;
    g_wnc_config.net_config.netmask  = DEF_NETMASK;
    g_wnc_config.net_config.pc_ip    = DEF_PC_IP;
    g_wnc_config.net_config.hvcs_ip  = DEF_HVCS_IP;
    
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)WNC_CONFIG_ADDR);
    rt_device_write(dev_ext_flash, WNC_CONFIG_ADDR, &g_wnc_config, sizeof(g_wnc_config));
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)WNC_CONFIG_BAK_ADDR);
    rt_device_write(dev_ext_flash, WNC_CONFIG_BAK_ADDR, &g_wnc_config, sizeof(g_wnc_config));
    
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)CONFIG_FILE_INFO_ADDR);
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_BLK_ERASE, (void *)LIGHT_CONNECT_FILE_ADDR);
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_BLK_ERASE, (void *)LIGHT_PARKING_FILE_ADDR);
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_BLK_ERASE, (void *)SENSOR_LIST_FILE_ADDR);
}

/**
 * @brief  perpare to use external flash with rt-thread device interface
 * @param  pointer to device name register to rt-thread
 */
void ext_flash_init(const char * flash_name)
{
    if ((dev_ext_flash = rt_device_find(flash_name)) == RT_NULL)
    {
        /* no this device */
        while(1);  /* wait for dog */
    }
    
    if(rt_device_open(dev_ext_flash, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        /* open failed */
    }
}

/**
 * @brief  get wnc configuration save on external flash
 */
void get_wnc_config(void)
{
    rt_device_read(dev_ext_flash, WNC_CONFIG_ADDR, &g_wnc_config, sizeof(g_wnc_config));
    if(check_wnc_config() == -1)
    {
        /* read settings from backup sector */
        rt_device_read(dev_ext_flash, WNC_CONFIG_BAK_ADDR, &g_wnc_config, sizeof(g_wnc_config));
        
        if(check_wnc_config() == -1)
        {
            /* initailize settings to default value */
            init_wnc_config();
        }
    }
    
    rt_memcpy(&g_wnc_config_bak, &g_wnc_config, sizeof(g_wnc_config));
}

/**
 * @brief  update wnc_config saved in external flash
 * @retval RT_EOK
 */
rt_err_t set_wnc_config(void)
{
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)WNC_CONFIG_ADDR);
    rt_device_write(dev_ext_flash, WNC_CONFIG_ADDR, &g_wnc_config_bak, sizeof(g_wnc_config));
    rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)WNC_CONFIG_BAK_ADDR);
    rt_device_write(dev_ext_flash, WNC_CONFIG_BAK_ADDR, &g_wnc_config_bak, sizeof(g_wnc_config));
    
    rt_memcpy(&g_wnc_config, &g_wnc_config_bak, sizeof(g_wnc_config));
    
    return RT_EOK;
}

/**
 * @brief   handle a download packet and save data to external flash
 * @param   pack_sn: current packet serial number
 * @param   data: pointer to data payload buffer
 * @param   data_len: payload length
 * @retval  DOWNLOAD_DOWNLOADING: current file downloading not finish yet
 *          DOWNLOAD_OVER: current file download success
 *          DOWNLOAD_ERR: find an error during download, stop this option 
 */
rt_uint8_t download_file(rt_uint8_t pack_sn, char *data, rt_uint16_t data_len)
{
    rt_uint8_t ret = DOWNLOAD_ERR;
    
    /* check packet serial number */
    if(++file_operate_state.packet_seq != pack_sn)
    {
        DEBUG_PRINTF("pack sn not correct, now %d, need %d\r\n", 
                      pack_sn, file_operate_state.packet_seq);
        return ret;
    }
    
    /* first packet, fine file informations */
    if(pack_sn == 1)
    {
        rt_uint8_t  file_name_len;
        char     file_name[256];
        rt_uint32_t file_size;

        data++;
        file_operate_state.state = FILE_OPT_STATE_DOWNLOAD;
        file_name_len = *data++;
        rt_memcpy(file_name, data, file_name_len);
        file_name[file_name_len] = '\0';
        data += file_name_len;
        file_size = *((rt_uint32_t*)data);
        file_size = ntohl(file_size);
        data += 4;
                        
        if(!process_download_type(file_name, file_size))
        {
            DEBUG_PRINTF("file type not support\r\n");
            return ret;
        }
        data_len -= (file_name_len + 6);        
    }
    DEBUG_PRINTF("do here test\r\n");
    rt_device_write(dev_ext_flash, file_operate_state.flash_addr, data, data_len);

	file_operate_state.flash_addr += data_len;
	file_operate_state.remain     -= data_len;
    
    if(file_operate_state.remain == 0)
    {
        finish_download_option();
        file_operate_reset();
        ret = DOWNLOAD_OVER;
		DEBUG_PRINTF("do DOWNLOAD_OVER test\r\n");
    }
    else
    {
        ret = DOWNLOAD_DOWNLOADING;
		DEBUG_PRINTF("do DOWNLOAD_DOWNLOADING test\r\n");
    }
    
    return ret;
}

/**
 * @brief   package file data to upload data buffer
 * @param   pack_sn: current packet serial number
 * @param   data: pointer to data payload buffer
 * @retval  size of data been packaged. 0 means file upload over; -1 means file upload failed 
 */
int16_t upload_file(rt_uint8_t pack_sn, char *data, rt_uint16_t buf_size)
{
    rt_int16_t  len;
    rt_uint16_t free_size;              /* free size in tcp send buffer */
    rt_size_t   read_size;
    char        *payload = data;
    
    /* first packet */
    if(pack_sn == 1)
    {
        rt_size_t   size;
        rt_uint8_t  name_len;
        char        tmp_name[MAX_FILE_NAME_LEN];
        
        data += 2;
        file_operate_state.state = FILE_OPT_STATE_UPLOAD;
        
        /* get file type */
        file_operate_state.file_type = get_file_type(data);
        if(file_operate_state.file_type == MAX_FILE_TYPE_NUM)
        {
            /* file type not support */
            return -1;
        }
        
        switch(file_operate_state.file_type)
        {
        case REGION_CONFIG_FILE:
            if(g_file_info_list.region_config.sn == 0xffffffff)
            {
                /* file not exist */
                return -1;
            }
            file_operate_state.flash_addr = REGION_CONFIG_FILE_ADDR;
            size = g_file_info_list.region_config.size;
            rt_memcpy(tmp_name, g_file_info_list.region_config.file_name, sizeof(tmp_name));
            name_len = rt_strlen(tmp_name);
            break;
        case LIGHT_CONNECT_FILE:
            if(g_file_info_list.light_connect.sn == 0xffffffff)
            {
                /* file not exist */
                return -1;
            }
            file_operate_state.flash_addr = LIGHT_CONNECT_FILE_ADDR;
            size = g_file_info_list.light_connect.size;
            rt_memcpy(tmp_name, g_file_info_list.light_connect.file_name, sizeof(tmp_name));
            name_len = rt_strlen(tmp_name);
            break;
        case LIGHT_PARKING_FILE:
            if(g_file_info_list.light_parking.sn == 0xffffffff)
            {
                /* file not exist */
                return -1;
            }
            file_operate_state.flash_addr = LIGHT_PARKING_FILE_ADDR;
            size = g_file_info_list.light_parking.size;
            rt_memcpy(tmp_name, g_file_info_list.light_parking.file_name, sizeof(tmp_name));
            name_len = rt_strlen(tmp_name);
            break;
        case SENSOR_LIST_FILE:
            if(g_file_info_list.sensor_list.sn == 0xffffffff)
            {
                /* file not exist */
                return -1;
            }
            file_operate_state.flash_addr = SENSOR_LIST_FILE_ADDR;
            size = g_file_info_list.sensor_list.size;
            rt_memcpy(tmp_name, g_file_info_list.sensor_list.file_name, sizeof(tmp_name));
            name_len = rt_strlen(tmp_name);
            break;
        case SYSTEM_LOG_FILE:
            size = prepare_read_system_log();
            name_len = rt_strlen(file_name_def[SYSTEM_LOG_FILE]);
            rt_memcpy(tmp_name, file_name_def[SYSTEM_LOG_FILE], name_len);                    
            break;
        case WORKSTATE_LOG_FILE:
            size = prepare_read_work_state_log();
            name_len = rt_strlen(file_name_def[WORKSTATE_LOG_FILE]);
            rt_memcpy(tmp_name, file_name_def[WORKSTATE_LOG_FILE], name_len);                    
            break;
        default:
            /* should never get here */
            return -1;
        }

        file_operate_state.remain = size;

		*payload++ = 0;
		*payload++ = name_len;
		rt_memcpy(payload, tmp_name, name_len);
		payload += name_len;
		size = htonl(size);
		rt_memcpy(payload, &size, 4);
		payload += 4;        
        
        len = name_len + 6;             /* 1 byte upload mark, 1 byte name_len, 4 bytes file size */        
        free_size = buf_size - len;
    }
    else /* followed packet */
    {
        len = 0;
        free_size = buf_size;
    }
    
    if(file_operate_state.remain > 0)
    {
        read_size = (free_size > file_operate_state.remain) ? 
                     file_operate_state.remain : free_size;
        read_size = read_file(payload, read_size);

        file_operate_state.remain     -= read_size;
        file_operate_state.flash_addr += read_size;
        len                           += read_size;
    }
    
    return len;
}

/**
 * @brief  reset file operate state when shut down current option
 */
void file_operate_reset(void)
{
    rt_memset(&file_operate_state, 0, sizeof(file_operate_state));
}

/**
 * @brief  initalize configure files interrelated lists
 */
void init_parking_lists(void)
{
    /* get configure files informations first */
    rt_device_read (dev_ext_flash, 
                    CONFIG_FILE_INFO_ADDR, 
                    &g_file_info_list, 
                    sizeof(g_file_info_list));
    
    /* these three files MUST read in the follow order */
    read_light_connect_file();
	read_sensor_list_flie();
	read_light_parking_flie();
}

/**
 * @brief  get configure file serial number
 * @retval serial number
 */
rt_uint32_t get_file_sn(void)
{
    return g_file_info_list.sensor_list.sn;
}

/**
 * @brief  confirm new software upgrade
 */
void upgrade_confirm(void)
{
	int i;
	FIRMUP_FLASHINFO		FirmUpFlashInfo;
	FIRMBACK_FLASHINFO		FirmBackupFlashInfo;
	rt_uint32_t rd_addr, wt_addr;
	rt_uint32_t size_left, tmp_size;
    
    rt_device_read (dev_ext_flash, 
                    FIRMUPINFO_BASE_ADDR, 
                    &FirmUpFlashInfo, 
                    sizeof(FirmUpFlashInfo));

	if((FirmUpFlashInfo.have_firm == 0xa5a5a5a5) &&
       (FirmUpFlashInfo.need_up   == 0x05050505) &&
       (FirmUpFlashInfo.success   == 0xffffffff))
    {
		if(FirmUpFlashInfo.verify != xor_verify((char*)&FirmUpFlashInfo, sizeof(FIRMUP_FLASHINFO) - 1))
        {
			return;
        }

		for(i = 0; i < FIRMWAREUPGRADE_SCT_NUM/16; i++)
		{
            rt_device_control(dev_ext_flash, 
                              GD_FLASH_CTRL_BLK_ERASE, 
                              (void *)((FIRMWAREBACKUP_BASE_SCT + i * 16) * FLASH_BYTES_PER_SECTOR));
            // TODO: clear dog
		}
		
		size_left = FirmUpFlashInfo.bin_len;
		rd_addr	= FIRMWAREUPGRADE_BASE_SCT * FLASH_BYTES_PER_SECTOR;
		wt_addr = FIRMWAREBACKUP_BASE_SCT * FLASH_BYTES_PER_SECTOR;

		while(size_left)
		{
            rt_uint8_t read_buf[1024];
            
            tmp_size = (size_left > 1024) ? 1024 : size_left;

            rt_device_read(dev_ext_flash, rd_addr, read_buf, tmp_size);
            rd_addr += tmp_size;

            rt_device_write(dev_ext_flash, wt_addr, read_buf, tmp_size);
            wt_addr += tmp_size;
            size_left -= tmp_size;
		}
        feed_dog();
        
		FirmBackupFlashInfo.useful = 0xa5a5a5a5;
		FirmBackupFlashInfo.bin_len = FirmUpFlashInfo.bin_len;
		rt_memcpy((void*)FirmBackupFlashInfo.ver, (void*)FirmUpFlashInfo.ver, 3);
        
        rt_device_control(dev_ext_flash, 
                          GD_FLASH_CTRL_SCT_ERASE, 
                          (void *)(FIRMBACKINFO_BASE_ADDR));
        rt_device_write(dev_ext_flash, 
                        FIRMBACKINFO_BASE_ADDR, 
                        &FirmBackupFlashInfo, 
                        sizeof(FirmBackupFlashInfo));

		FirmUpFlashInfo.success = 0xa5a5a5a5;
		FirmUpFlashInfo.verify	= xor_verify((char*)&FirmUpFlashInfo, sizeof(FirmUpFlashInfo) - 1);	
        rt_device_control(dev_ext_flash, 
                          GD_FLASH_CTRL_SCT_ERASE, 
                          (void *)(FIRMUPINFO_BASE_ADDR));
        rt_device_write(dev_ext_flash, 
                        FIRMUPINFO_BASE_ADDR, 
                        &FirmUpFlashInfo, 
                        sizeof(FirmUpFlashInfo));

		add_log("upgrade new software ok");
	}
}

/* ****************************** end of file ****************************** */
