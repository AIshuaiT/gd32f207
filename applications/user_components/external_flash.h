/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : external_flash.h
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

#ifndef __EXTERNAL_FLASH_H__
#define __EXTERNAL_FLASH_H__

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "spi_flash_gd.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */

#define DOWNLOAD_DOWNLOADING        (2)
#define DOWNLOAD_OVER               (1)
#define DOWNLOAD_ERR                (0)

/* configure files and log files */
#define CONFIG_FILE_INFO_BASE_SCT   (0)
#define CONFIG_FILE_INFO_ADDR       (CONFIG_FILE_INFO_BASE_SCT * FLASH_BYTES_PER_SECTOR)

/* every configure file at most use 16 sectors */
#define CONFIG_FILE_SCT_NUM         (16)
/* every configure file less than 64KB */
#define MAX_CONFIG_FILE_SIZE        (CONFIG_FILE_SCT_NUM * FLASH_BYTES_PER_SECTOR)

/* LN_region_config_#sn.ini */
#define REGION_CONFIG_BASE_SCT      (16)
#define REGION_CONFIG_FILE_ADDR     (REGION_CONFIG_BASE_SCT * FLASH_BYTES_PER_SECTOR)
/* LN_light_connect_#sn.ini */
#define LIGHT_CONNECT_BASE_SCT      (32)
#define LIGHT_CONNECT_FILE_ADDR     (LIGHT_CONNECT_BASE_SCT * FLASH_BYTES_PER_SECTOR)
/* LN_light_parking_#sn.ini */
#define LIGHT_PARKING_BASE_SCT      (48)
#define LIGHT_PARKING_FILE_ADDR     (LIGHT_PARKING_BASE_SCT * FLASH_BYTES_PER_SECTOR)
/* LN_sensor_#sn.ini */
#define SENSOR_LIST_BASE_SCT        (64)
#define SENSOR_LIST_FILE_ADDR       (SENSOR_LIST_BASE_SCT * FLASH_BYTES_PER_SECTOR)

#define SYSTEM_LOG_BASE_SCT         (80)
#define SYSTEM_LOG_SCT_NUM          (16)
#define SYSTEM_LOG_BASE_ADDR        (SYSTEM_LOG_BASE_SCT * FLASH_BYTES_PER_SECTOR)
#define SYSTEM_LOG_MAX_SIZE         (SYSTEM_LOG_SCT_NUM * FLASH_BYTES_PER_SECTOR)

#define WORK_STATE_LOG_BASE_SCT	    (112)
#define WORK_STATE_LOG_SCT_NUM      (32)
#define WORK_STATE_LOG_BASE_ADDR    (WORK_STATE_LOG_BASE_SCT * FLASH_BYTES_PER_SECTOR)
#define WORK_STATE_LOG_MAX_SIZE     (WORK_STATE_LOG_SCT_NUM * FLASH_BYTES_PER_SECTOR)

#define RESERVED_PARKING_BASE_SCT	(160)
#define RESERVED_PARKING_FILE_ADDR  (RESERVED_PARKING_BASE_SCT * FLASH_BYTES_PER_SECTOR)

/* upgrade files */
#define FIRMWAREUPGRADE_BASE_SCT    (864)
#define FIRMWAREBACKUP_BASE_SCT     (928)
#define FIRMWAREUPGRADE_SCT_NUM	    (64)

#define	FIRMUPINFO_BASE_SCT		    (992)
#define FIRMUPINFO_BASE_ADDR        (FIRMUPINFO_BASE_SCT * FLASH_BYTES_PER_SECTOR)
#define	FIRMBACKINFO_BASE_SCT	    (993)
#define FIRMBACKINFO_BASE_ADDR      (FIRMBACKINFO_BASE_SCT * FLASH_BYTES_PER_SECTOR)

/* wnc config */
#define WNC_CONFIG_SCT	            (999)
#define WNC_CONFIG_ADDR             (WNC_CONFIG_SCT * FLASH_BYTES_PER_SECTOR)
#define WNC_CONFIG_BAK_SCT          (1000)
#define WNC_CONFIG_BAK_ADDR         (WNC_CONFIG_BAK_SCT * FLASH_BYTES_PER_SECTOR)

/* file types */
enum file_types
{
	REGION_CONFIG_FILE = 0,
	LIGHT_CONNECT_FILE,
	LIGHT_PARKING_FILE,
	SENSOR_LIST_FILE,
	UPGRADE_BIN_FILE,
	SYSTEM_LOG_FILE,
	WORKSTATE_LOG_FILE,
	RESERVED_PARKING,
	MAX_FILE_TYPE_NUM ,
};

/* IP address mode */
#define DHCP_MODE                       (0)
#define DEDICATED_MODE                  (1)

/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containing network configuration parameters
 */
__packed typedef struct
{
	rt_uint8_t             net_mode;            /* 0: dhcp, 1: dedicated IP */
    /* parameters for dedicated IP */
	rt_uint32_t            ip;
    rt_uint32_t            gateway;
    rt_uint32_t            netmask;
    /* remote ip use for cross network segment */
	rt_uint32_t            pc_ip;
    rt_uint32_t            hvcs_ip;
} net_cfg_t; 

/**
 * @brief  structure containing lora user configuration parameters
 */
__packed typedef struct
{
	rt_uint8_t             threshold;           /* reserved */
	rt_uint32_t            rf_freq;             /* lora module refrence frequncy */
	rt_uint8_t             rf_power;            /* lora mocule send power */
    rt_uint16_t            time_offline;        /* lora node not communication to         */
                                                /* concentrator longger than time_offline */
                                                /* seemed as offline                      */
} lora_cfg_t; 

/**
 * @brief  structure containing wnc configuration parameters
 */
__packed typedef struct
{
	rt_uint32_t			version;
	net_cfg_t           net_config;
	lora_cfg_t          lora_config;
} wnc_cfg_t;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

extern wnc_cfg_t    g_wnc_config;
extern wnc_cfg_t    g_wnc_config_bak;
extern const char   file_name_def[MAX_FILE_TYPE_NUM][24];
 
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

extern void         ext_flash_init      (const char * flash_name);
extern void         get_wnc_config      (void);
extern void         user_init           (void);
extern rt_err_t     set_wnc_config      (void);
extern rt_int16_t   upload_file         (rt_uint8_t pack_sn, char *data, rt_uint16_t buf_size);
extern rt_uint8_t   download_file       (rt_uint8_t pack_sn, char *data, rt_uint16_t data_len);
extern void         file_operate_reset  (void);
extern void         init_parking_lists  (void);
extern rt_uint32_t  get_file_sn         (void);
extern void         upgrade_confirm     (void);

/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#endif /* __EXTERNAL_FLASH_H__ */
 
/* ****************************** end of file ****************************** */
