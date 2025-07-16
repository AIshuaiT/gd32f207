/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : main.c
 * Arthor    : Test
 * Date      : April 5th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-05      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include <rthw.h>
#include <rtthread.h> 

#include "board.h"

/**
 ******************************************************************************
 *                                   MACROS
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
 
static void rtthread_startup(void);
 
/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern int rt_application_init(void);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

/**
 * @brief  This function will startup RT-Thread RTOS.
 */
static void rtthread_startup(void)
{
    /* initailize board */
    rt_hw_board_init();

    /* show version */
    rt_show_version();

    /* init timer system */
    rt_system_timer_init();

#ifdef RT_USING_HEAP
    rt_system_heap_init((void*)GD32_SRAM_BEGIN, (void*)GD32_SRAM_END);
#endif

    /* init scheduler system */
    rt_system_scheduler_init();

    /* init application */
    rt_application_init();

    /* init timer thread */
    rt_system_timer_thread_init();

    /* init idle thread */
    rt_thread_idle_init();

    /* start scheduler */
    rt_system_scheduler_start();

    /* never reach here */
    return ;
}

/**
 * MAIN ENTRY
 */
int main(void)
{
    /* disable interrupt first */
    rt_hw_interrupt_disable();

    /* startup RT-Thread RTOS */
    rtthread_startup();
    
    /* should never reach here */
    return 0;
}

 
/* ****************************** end of file ****************************** */
