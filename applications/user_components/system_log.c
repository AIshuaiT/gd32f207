/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : system_log.c
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

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

 
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
 
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief  write log string to system log
 * @param  data: pointer to log string buffer
 */
void add_log(const char *data)
{
    RTC_T       rtc;
    char        str_log[256];
    rt_uint32_t len;
    
    pcf8563_get_datetime(&rtc);
    
    len = rt_snprintf(str_log, sizeof(str_log),
            "%04d-%02d-%02d %02d:%02d:%02d  %s\r\n",
            rtc.year, rtc.month, rtc.day, rtc.hour, rtc.minute, rtc.second, data);
    
    log_write(str_log, len, SYSTEM_LOG);
}

/**
 * @brief  perpare to read system log
 * @retval file size
 */
rt_size_t prepare_read_system_log(void)
{
    rt_size_t size;
    
    prepare_read(SYSTEM_LOG);
    size = calculate_file_size(SYSTEM_LOG);
    
    return size;
}

/**
 * @brief  read system log data to buffer
 * @param  buf: pointer to data buffer
 * @param  max_size: data size want to read
 * @retval size read out
 */
rt_size_t read_system_log(char * buf, rt_uint32_t max_size)
{
    return log_read(buf, max_size, SYSTEM_LOG);
}

 
/* ****************************** end of file ****************************** */
