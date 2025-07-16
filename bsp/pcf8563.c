/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : pcf8563.c
 * Arthor    : Test
 * Date      : May 11th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-11      Test          Modify from older version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */
 
#include "pcf8563.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */

/* datatime limits */
#define RTC_YEAR_MIN	2009 
#define RTC_YEAR_MAX	2099 
#define RTC_MONTH_MIN   1
#define RTC_DAY_MIN   	1
 
#define RTC_HOUR_MIN   	0
#define RTC_MINUTE_MIN 	0
#define RTC_SECOND_MIN  0
 
/* Base day (1901.1.1 DOW = 2), which is used for day calculate */ 
#define RTC_BASEYEAR	1901 
#define RTC_BASEMONTH	1 
#define RTC_BASEDAY		1 
#define RTC_BASEDOW		2
 
 
/* for debug */
#define DEBUG_PCF8563   0

#if DEBUG_PCF8563
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
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static rt_device_t dev_rtc = RT_NULL;

static const rt_uint8_t RTC_MonthVal[13] = \
{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static rt_uint8_t   is_leap_year        (rt_uint16_t year);
static int          GetDOY              (rt_uint16_t year, rt_uint8_t month, rt_uint8_t day);
static rt_uint8_t   GetDOW              (rt_uint16_t year, rt_uint8_t month, rt_uint8_t day);
static rt_err_t     check_datetime      (RTC_T *_tRtc);
static rt_uint8_t   pcf8563_ByteToPbcd  (rt_uint8_t value);
static rt_uint8_t   pcf8563_PbcdToByte  (rt_uint8_t bcd);
 
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
 * @brief  check if the year is a leap year
 * @param  year: year to chek
 * @retval 1:true, 0:false
 */
static rt_uint8_t is_leap_year(rt_uint16_t year)
{
	if ((!(year%4) && (year%100)) || !(year%400))
	{
		return 1;
	}
	else
	{
		return 0;
	}   
}

/**
 * @brief  Get the day of year according to the date
 * @param  year: year
 * @param  month: month
 * @param  day: day
 * @retval day of the year
 */
static int GetDOY(rt_uint16_t year, rt_uint8_t month, rt_uint8_t day)
{
	int DOY = 0, i;
	
	for(i = 1; i < month; i++)
    {
		DOY += RTC_MonthVal[i-1];
    }
    
	if((month > 2) && is_leap_year(year)) DOY++;
    
	return (DOY + day);
}

/**
 * @breif  Get the day of week according to the date
 * @param  year: year
 * @param  month: month
 * @param  day: day
 * @retval day of week
 *
 * @NOTE   Year is not smaller than RTC_YEARMIN
 */
static rt_uint8_t GetDOW(rt_uint16_t year, rt_uint8_t month, rt_uint8_t day)
{
	int i, DOW = 0;
	
	for (i = RTC_BASEYEAR; i < year; i++)
	{
		DOW += 365;
		if(is_leap_year(i)) DOW++;
	}
	
	DOW += GetDOY(year, month, day) - 1;
	DOW  = (DOW + RTC_BASEDOW) % 7;
	
	return ((rt_uint8_t)DOW);   
}

/**
 * @brief  check if the datatime is valid
 * @param  _tRtc: pointer to datatime structure
 * @retval RT_EOK is valid, others is invalid
 */
static rt_err_t check_datetime(RTC_T *_tRtc)
{
	/* check year */
	if(_tRtc->year < RTC_YEAR_MIN || _tRtc->year > RTC_YEAR_MAX)
	{
		return RT_ERROR;
	}

	/* check month */
	if((_tRtc->month < 1) || (_tRtc->month > 12))
	{
		return RT_ERROR;
	}
    
	/* check day */
	if((_tRtc->day < 1) || (_tRtc->day > 31))
	{
		return RT_ERROR;
	}
	if( _tRtc->day > RTC_MonthVal[_tRtc->month])
	{
		if(_tRtc->month == 2)
		{
			if (is_leap_year(_tRtc->year) && (_tRtc->day > 29))
            {
                return RT_ERROR;
			}
			else if (_tRtc->day > 28)
			{
				return RT_ERROR;
			}   
		}
		else
		{
			return RT_ERROR;
		}   
	}
    
	/* check time */
	if ((_tRtc->hour > 23) || (_tRtc->minute > 59) || (_tRtc->second > 59))
	{
		return RT_ERROR;
	}
    
	return RT_EOK;   
}

/**
 * @brief  transform hex to BCD code
 * @param  value: value
 * @retval BCD code
 */
static rt_uint8_t pcf8563_ByteToPbcd(rt_uint8_t value)
{
	return (((value / 10) << 4) + (value % 10));
}

/**
 * @brief  transform BCD to hex code
 * @param  bcd: bcd
 * @retval hex
 */
static rt_uint8_t pcf8563_PbcdToByte(rt_uint8_t bcd)
{
	return ((10 * (bcd >> 4)) + (bcd & 0x0f));   
}

/**
 * @brief  initailize pcf8563 device
 */
rt_err_t pcf8563_init(void)
{
    if((dev_rtc = rt_device_find("i2c1")) == RT_NULL)
    {
        /* device not found */
    }
    
    if(rt_device_open(dev_rtc, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        /* open failed */
    }
    
    return RT_EOK;
}

/**
 * @breif  set datatime to pcf8563
 * @param  _tRtc: pointer to datatime structure
 * @retval RT_EOK, other means set failed
 */
rt_err_t pcf8563_set_datetime(RTC_T *_tRtc)
{
    rt_uint8_t buf[16];
    
    /* check rtt device */
    if(dev_rtc == RT_NULL)
    {
        return RT_ERROR;
    }
    
    /* check valid */
	if(check_datetime(_tRtc) != RT_EOK)
	{
		return RT_ERROR;
	}
    
    /* set day of week */
	_tRtc->week = GetDOW(_tRtc->year,_tRtc->month, _tRtc->day);
    
    /* restore buffer */
    /* control and status reigsters */
	buf[0] = 0;
	buf[1] = 0;
    /* time and date registers */
	buf[2] = pcf8563_ByteToPbcd(_tRtc->second);                 /* 0 - 59 */
	buf[3] = pcf8563_ByteToPbcd(_tRtc->minute);                 /* 0 - 59 */
	buf[4] = pcf8563_ByteToPbcd(_tRtc->hour);                   /* 0 - 23 */
	buf[5] = pcf8563_ByteToPbcd(_tRtc->day);                    /* 1 - 31 */
	buf[6] = pcf8563_ByteToPbcd(_tRtc->week);                   /* 0 - 6 */	
	buf[7] = pcf8563_ByteToPbcd(_tRtc->month);	                /* 1 - 12 */
	buf[8] = pcf8563_ByteToPbcd((_tRtc->year - 2000) & 0xFF);   /* 0 - 99 */
    /* other registers see pcf8563 datasheet */
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x01;
	buf[12] = 0x00;
	buf[13] = 0x00;
	buf[14] = 0x00;
	buf[15] = 0x00;

	if(rt_device_write(dev_rtc, 0, buf, 16) == 0)
    {
        return RT_ERROR;
    }
    
    return RT_EOK;
}

/**
 * @breif  read datatime from pcf8563
 * @param  _tRtc: pointer to datatime structure
 * @retval RT_EOK, other means read failed
 */
rt_err_t pcf8563_get_datetime(RTC_T *_tRtc)
{
	rt_uint8_t buf[16];

    /* check rtt device */
    if(dev_rtc == RT_NULL)
    {
        return RT_ERROR;
    }
	
	if (rt_device_read(dev_rtc, 2, buf, 7) == 0)
	{
		return RT_ERROR;   
	}
    
	_tRtc->second = pcf8563_PbcdToByte(buf[0] & 0x7F);
	_tRtc->minute = pcf8563_PbcdToByte(buf[1] & 0x7F);
	_tRtc->hour   = pcf8563_PbcdToByte(buf[2] & 0x3F);
	_tRtc->day    = pcf8563_PbcdToByte(buf[3] & 0x3F);
	_tRtc->month  = pcf8563_PbcdToByte(buf[5] & 0x1F);
	_tRtc->year   = pcf8563_PbcdToByte(buf[6]) + 2000;
	
	return RT_EOK;   
}
 
/* ****************************** end of file ****************************** */
