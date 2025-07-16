/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_network.h
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

#ifndef __THREAD_NETWORK_H__
#define __THREAD_NETWORK_H__

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include <rtthread.h>

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define REMOTE_UDP_SERVER_PORT          (5210)

/**
 * @NOTE  must less than TCP_MSS definitions in "lwipopt.h", 
 *        set to same size is better
 */
#define MAX_TCP_DATA_LENGTH             (1024)

/* TCP commands */
#define CMD_SET_DEV_ID_AND_MAC          1
#define CMD_ERASE_DEV_ID_AND_MAC        2
#define CMD_SOFT_REBOOT                 3
#define CMD_ADJUST_MACHINE_CLOCK        6
#define CMD_READ_MACHINE_CLOCK          7
#define CMD_SET_SERVER_IP               9
#define CMD_CONFIGURE_FILES_OPT         15
#define CMD_NET_PARAMS_OPT              23
#define CMD_GET_DEVICE_ALL_INFOS        24
#define CMD_GET_SLOT_INFOS              31
#define CMD_LOCAL_LORA_PARAMS_OPT	    38
#define CMD_SENSOR_OFFLINE_TIMEOUT      39
#define CMD_LORA_SEND_TEST              240
#define CMD_USER_INIT                   241 
#define CMD_CLIENT_NODE_LORA_SEND       242
#define CMD_SET_LIGHT_COLOR             243
#define CMD_GET_LORA_CLIENT_ID		    244
#define CMD_LORA_RECV_TEST              245
#define CMD_LORA_CONFIG_NODE_BY_RANGE   246
#define CMD_LORA_INIT_NODE              247
#define CMD_LORA_CONFIG_NODE_BY_ID      248

#define DEVICE_TYPE                     "LN"
 
/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containing tcp commands header
 */
__packed struct tcp_pack_header
{
	rt_uint8_t     cmd;                    /* command value */
	rt_uint32_t    id;                     /* target device id */
	char           device_type[4];         /* target device type string */
	rt_uint8_t     pack_sn;                /* serial number for continuous command */
	rt_uint16_t    data_len;               /* payload length */
};
typedef struct tcp_pack_header * tcp_pack_header_t;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

extern rt_thread_t  tid_udp_client;  /* udp client thread handler */
extern rt_thread_t  tid_udp_server;  /* udp server thread handler */
extern rt_thread_t  tid_tcp_server;  /* tcp server thread handler */

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

extern char     xor_verify                  (const char *data, int len);
extern void     get_datetime                (char *data);
extern int      init_udp_socket_handler     (void);
extern void     thread_udp_client           (void* parameter);
extern void     thread_udp_server           (void* parameter);
extern void     thread_tcp_server           (void* parameter);

/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */


#endif /* __THREAD_NETWORK_H__ */
 
/* ****************************** end of file ****************************** */
