/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : wnc_data_base.h
 * Arthor    : Test
 * Date      : Apr 24th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-24      Test          First version. Copy from "WNC_struct_def.h"
 ******************************************************************************
 */

#ifndef __WNC_DATA_BASE_H__
#define __WNC_DATA_BASE_H__

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

/* limitations */
#define MAX_DETECTOR_PER_WNC            (60)
#define MAX_NODE_WHOLE_PARKING_LOT      (1024)
#define MAX_LIGHT_PER_WNC               (40)
#define MAX_RELATION_MASK               (MAX_LIGHT_PER_WNC/8)
#define MAX_AVG_NUM                     (10)

/* device type */
#define NODE_DEVICE_TYPE_DETECTOR       (0)
#define NODE_DEVICE_TYPE_LIGHT          (1)

/* device state */
#define NODE_STATE_OFFLINE              (251)
#define NODE_STATE_NEW_DEVICE           (252)

/* light color */
#define LIGHT_COLOR_OFF                 (0)
#define LIGHT_COLOR_RED                 (1)
#define LIGHT_COLOR_GREEN               (2)
#define LIGHT_COLOR_BLUE                (3)
#define LIGHT_COLOR_YELLOW              (4)
#define LIGHT_COLOR_CYAN                (5)
#define LIGHT_COLOR_PURPLE              (6)
#define LIGHT_COLOR_WHITE               (7)
#define LIGHT_COLOR_BLINK_RED	        (11)
#define LIGHT_COLOR_BLINK_GREEN         (12)
#define LIGHT_COLOR_BLINK_BLUE          (13)
#define LIGHT_COLOR_BLINK_YELLOW        (14)
#define LIGHT_COLOR_BLINK_CYAN          (15)
#define LIGHT_COLOR_BLINK_PURPLE        (16)
#define LIGHT_COLOR_BLINK_WHITE         (17)


/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containing single parking detector informations
 */
struct single_detector_info
{
	unsigned int   id;                          /* detector id */
	unsigned char  state;                       /* parking state: */
                                                /* 1: occupied */  
                                                /* 0: free */
                                                /* NODE_STATE_OFFLINE: device offline */    
	unsigned char  value;                       /* detect value */
	unsigned char  battery;                     /* battery power */
    unsigned char  lowest_power;                /* loewt power concentrator known */
	signed   char  rssi;                        /* rssi on detector side */
	signed   char  snr;                         /* snr on detector side */
	unsigned char  cnt;                         /* detector lora send counter, 0 - 255 cycle */ 
	unsigned char  interval;                    /* minutes since lase lora communication */ 
	unsigned char  is_park_state_changed;       /* flag express parking state is changed */
	unsigned char  recv_pack_flag;              /* flag express concentrator received packet */
                                                /* from this detector before */
	/* use for statistics */
	unsigned int   resend1;                     /* number of resend once packet */
	unsigned int   resend2;                     /* number of resend twice packet */
	unsigned int   resend3;                     /* number of resend three times packet */
	unsigned int   first_cnt;                   /* first cnt concentrator received since power on */
	unsigned int   offline_time;                /* detector offline time counter */
	unsigned int   reset_time;                  /* detector restart time counter */
    unsigned int   send_num;                    /* number of packets detector sended */    
	unsigned int   recv_num;                    /* number of packets concentrator received */
	unsigned int   abs_recv_num;                /* number of packets concentrator received without resend */
	unsigned int   miss_send_num;               /* number of packets detector loss */
	unsigned int   miss_recv_num;               /* number of packets concentrator loss */
	unsigned int   send_num_before_this_time;   /* totle packets since concentrator power on to detector restart */
	unsigned int   rounds;                      /* cycle counter of cnt */

    /* use for work state log */
    unsigned int  change_time;                  /* park state changed times during this hour */
    unsigned int  last_resend1;                 /* number of resend once packet before this hour */
    unsigned int  last_resend2;                 /* number of resend twice packet before this hour */
    unsigned int  last_resend3;                 /* number of resend three times packet before this hour */
    unsigned int  last_offline;                 /* detector offline time counter before this hour */
    unsigned int  last_reset;                   /* detector restart time counter before this hour */
    unsigned int  last_send;                    /* number of packets detector sended before this hour */
    unsigned int  last_miss_recv;               /* number of packets concentrator loss before this hour */
    
    /* use for system log */
    unsigned char is_in_config;                 /* use as bool, if this detector entering configuration mode */
    
    /* use for LoRa ADR */
	unsigned char  current_dr;                  /* current datarate (SF in LoRa mode) */
	/* on concentrator side */    
	unsigned char  c_ptr;                       /* pointer to c_rssi[] and c_snr[] */
	signed 	 short c_rssi[MAX_AVG_NUM];         /* buffer save last MAX_AVG_NUM packets' rssi */
	signed   short c_avg_rssi;                  /* average rssi of last MAX_AVG_NUM packets */ 
	signed   char  c_snr[MAX_AVG_NUM];          /* buffer save last MAX_AVG_NUM packets' snr */
	signed   char  c_avg_snr;                   /* average rsnr of last MAX_AVG_NUM packets */
};
typedef struct single_detector_info single_detector_info_t;

/**
 * @brief  structure containing informations of all detectors belone to this concentrator
 */
struct detector_info_list
{
	unsigned int            num;
	single_detector_info_t  detector_info[MAX_DETECTOR_PER_WNC];
};
typedef struct detector_info_list detector_info_list_t;

/**
 * @brief  structure described relationship of one detector and lights
 */
struct single_relation
{
	unsigned int  detector_id;
	unsigned char mask[MAX_RELATION_MASK];      /* one bit for one light */
};
typedef struct single_relation single_relation_t;

/**
 * @brief  structure containing relationships of all detectors 
 *         and lights belone to this concentrator
 */
struct relation_list
{   
	unsigned int        light_num;
	unsigned int        light_id[MAX_LIGHT_PER_WNC];
	unsigned int        empty[MAX_LIGHT_PER_WNC];         /* free parking number array */
    unsigned int        detector_num;
	single_relation_t   relation[MAX_DETECTOR_PER_WNC];
};
typedef struct relation_list relation_list_t;

/**
 * @brief  structure containing single light informations
 */
struct single_light_info
{
	unsigned int  id;                   /* light id */
	unsigned char state;                /* light device state */
                                        /* 0: device online */
                                        /* NODE_STATE_OFFLINE: device offline */
	signed   char rssi;                 /* rssi on light side */
	signed   char snr;                  /* snr on light side */
	unsigned char correct_color;        /* the color light should be */
	unsigned char current_color;        /* the color light is being */
	unsigned char interval;             /* minutes since lase lora communication */
	unsigned char time_left;            /* 0 means control light failure, light is offline */ 
};
typedef struct single_light_info single_light_info_t;

/**
 * @brief  structure containing informations of all lights belone to this concentrator
 */
struct light_info_list
{
	unsigned int         num;
	single_light_info_t  light_info[MAX_LIGHT_PER_WNC];
};
typedef struct light_info_list light_info_list_t;

/**
 * @brief  structure containing single new node device information
 */
struct single_new_node
{
	unsigned int    id;
	unsigned char   device_type;
	signed   short  rssi;                   /* rssi on concentrator side */
    signed   char   snr;                    /* snr on concentrator side */
	unsigned char   time_left;              /* udp need report times */
};
typedef struct single_new_node single_new_node_t;

/**
 * @brief  structure containing informations of all node device NOT belone to 
 *         this concentrator but have been received
 */
struct new_node_list
{
	unsigned short     num;
	single_new_node_t  node[MAX_NODE_WHOLE_PARKING_LOT];
};
typedef struct new_node_list new_node_list_t;

/**
 * @brief  sturcture containing single node device need config through LoRa
 */
struct single_config_node
{
    unsigned int    id;
    unsigned char   done;           /* when the byte set means this node set done */
};
typedef struct single_config_node single_config_node_t;

/**
 * @brief  structure containing node devices need config parameters through LoRa
 */
struct config_node_list
{
    unsigned short          num;    
    unsigned int            freq;               /* in Hz */
    unsigned short          period;             /* in second */
    single_config_node_t    node[MAX_NODE_WHOLE_PARKING_LOT];
};
typedef struct config_node_list config_node_list_t;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

/* wnc work lists */
extern detector_info_list_t    g_detector_info_list;
extern relation_list_t         g_relation_list;
extern light_info_list_t       g_light_info_list;
extern new_node_list_t         g_new_node_list;
extern config_node_list_t      g_config_node_list;
 
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
 
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#endif /* __WNC_DATA_BASE_H__ */
 
/* ****************************** end of file ****************************** */
