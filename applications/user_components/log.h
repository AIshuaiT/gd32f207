/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : log.h
 * Arthor    : Test
 * Date      : May 18th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-18      Test          First version.
 ******************************************************************************
 */

#ifndef __LOG_H__
#define __LOG_H__

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

#define TIME_WRITE_WORK_STATE_LOG       (1 * 60 * 60)       /* 1 hour */

enum log_type
{
    SYSTEM_LOG = 0,
    WORK_STATE_LOG,
    MAX_LOG_TYPE,
};

enum log_event_type
{
    EVENT_DETECTOR_OFFLINE = 0,
    EVENT_DETECTOR_RESTART,
    EVENT_LIGHT_OFFLINE,
	MAX_EVENT_TYPE,
};
 
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

/* log base functions */
extern void         logs_init               (void);
extern void         log_write               (char *buf, rt_uint32_t size, enum log_type type);
extern void         prepare_read            (enum log_type type);
extern rt_size_t    calculate_file_size     (enum log_type type);
extern rt_size_t    log_read                (char *buf, rt_uint32_t buf_size, enum log_type type);

/* system log */
extern void         add_log                 (const char *data);
extern rt_size_t    prepare_read_system_log (void);
extern rt_size_t    read_system_log         (char * buf, rt_uint32_t max_size);

/* work state log */
extern rt_size_t    prepare_read_work_state_log     (void);
extern rt_size_t    read_work_state_log             (char * buf, rt_uint32_t max_size);
extern void         work_state_power_on             (void);
extern void         work_state_time_period          (void);
extern rt_int32_t   work_state_log_add_event        (enum log_event_type event);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#endif /* __LOG_H__ */
 
/* ****************************** end of file ****************************** */
