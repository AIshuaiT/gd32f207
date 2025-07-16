/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : watchdog.c
 * Arthor    : Test
 * Date      : May 23th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-23      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "gd32f20x.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define WATCHDOG_CLK            RCC_APB2PERIPH_GPIOD
#define WATCHDOG_PORT           GPIOD
#define WATCHDOG_PIN            GPIO_PIN_15
 
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
 * @brief  initailize watchdog hardware
 */
void rt_hw_init_watchdog(void)
{
    GPIO_InitPara GPIO_InitStructure;

    RCC_APB2PeriphClock_Enable(WATCHDOG_CLK, ENABLE);
    
    GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUT_PP;
    
    GPIO_InitStructure.GPIO_Pin = WATCHDOG_PIN;
    GPIO_Init(WATCHDOG_PORT, &GPIO_InitStructure);    
}
    
/**
 * @brief  fecht hardware watchdog
 */
void feed_dog(void)
{
    uint16_t pins;
    
	pins =  GPIO_ReadOutputData(WATCHDOG_PORT);
	if(pins & WATCHDOG_PIN)
    {
		GPIO_ResetBits(WATCHDOG_PORT, WATCHDOG_PIN);
	}
	else
    {
		GPIO_SetBits(WATCHDOG_PORT, WATCHDOG_PIN);
	}
}

 
/* ****************************** end of file ****************************** */
