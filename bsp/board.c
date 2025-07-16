/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : board.c
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

#include <rtthread.h>

#include "board.h"
#include "usart.h"
#include "loragw.h"
#include "thread_led.h"

#include "gd32f20x.h"

#ifdef RT_USING_LWIP
#include "gd32f20x_eth.h"
#endif /* RT_USING_LWIP */

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define PCF8563_ADDR        (0xa2)

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
 
#ifdef RT_USING_SPI
#include "rt_gd32f20x_spi.h"

static void rt_hw_spi_init      (void);
#endif /* RT_USING_SPI */    

#ifdef RT_USING_I2C
#include "rt_gd32f20x_i2c.h"

static void rt_hw_i2c_init      (void);
#endif /* RT_USING_I2C */

static void NVIC_Configuration  (void);

/**
 ******************************************************************************
 *                         GLOBAL FUNCTION DECLARATION
 ******************************************************************************
 */

extern void feed_dog            (void);
 
/**
 ******************************************************************************
 *                                  FUNCTIONS
 ******************************************************************************
 */

#ifdef RT_USING_SPI
/**
 * @brief  initialize spi interface of gd32f207.
 *         spi1 for flash gd25q32.
 *         spi3 for sx1301 module.
 */
static void rt_hw_spi_init(void)
{
#ifdef RT_USING_GD_FLASH
    /* register spi bus */
    {
        static struct gd32_spi_bus gd32_spi;
        GPIO_InitPara GPIO_InitStructure;

        /* Enable GPIO clock */
        RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA | RCC_APB2PERIPH_AF,
                                   ENABLE);

        GPIO_InitStructure.GPIO_Pin   = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
        GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_AF_PP;
        GPIO_Init(GPIOA, &GPIO_InitStructure);

        gd32_spi_register(SPI1, &gd32_spi, "spi1");
    }

    /* attach cs */
    {
        static struct rt_spi_device spi_device;
        static struct gd32_spi_cs  spi_cs;

        GPIO_InitPara GPIO_InitStructure;

        GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
        GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUT_PP;

        spi_cs.GPIOx = GPIOB;
        spi_cs.GPIO_Pin = GPIO_PIN_1;
        RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOB, ENABLE);

        GPIO_InitStructure.GPIO_Pin = spi_cs.GPIO_Pin;
        GPIO_SetBits(spi_cs.GPIOx, spi_cs.GPIO_Pin);
        GPIO_Init(spi_cs.GPIOx, &GPIO_InitStructure);

        rt_spi_bus_attach_device(&spi_device, "spi1_0", "spi1", (void*)&spi_cs);
    }
#endif /* RT_USING_GD_FLASH */
    
#ifdef RT_USING_LORA
    /* register spi bus */
    {
        static struct gd32_spi_bus gd32_spi;
        GPIO_InitPara GPIO_InitStructure;

        /* Enable GPIO clock */
        RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOC | RCC_APB2PERIPH_AF,
                                   ENABLE);

        GPIO_InitStructure.GPIO_Pin   = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
        GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_AF_PP;
        GPIO_Init(GPIOC, &GPIO_InitStructure);
        
        GPIO_PinRemapConfig(GPIO_REMAP_SPI3 ,ENABLE);

        gd32_spi_register(SPI3, &gd32_spi, "spi3");
    }

    /* attach cs */
    {
        static struct rt_spi_device spi_device;
        static struct gd32_spi_cs  spi_cs;

        GPIO_InitPara GPIO_InitStructure;

        GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
        GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUT_PP;

        spi_cs.GPIOx = GPIOA;
        spi_cs.GPIO_Pin = GPIO_PIN_15;
        RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA | RCC_APB2PERIPH_AF, 
                                   ENABLE);

        GPIO_InitStructure.GPIO_Pin = spi_cs.GPIO_Pin;
        GPIO_SetBits(spi_cs.GPIOx, spi_cs.GPIO_Pin);
        GPIO_Init(spi_cs.GPIOx, &GPIO_InitStructure);
        
        GPIO_PinRemapConfig( GPIO_REMAP_SWJ_JTAGDISABLE, ENABLE );

        rt_spi_bus_attach_device(&spi_device, "spi3_0", "spi3", (void*)&spi_cs);
    }
#endif /* RT_USING_LORA */

}
#endif /* RT_USING_SPI */

#ifdef RT_USING_I2C
/**
 * @brief  initialize i2c interface of gd32f207.
 *         i2c1 for pcf8563
 */
static void rt_hw_i2c_init(void)
{
    {
        /* pcf8563 */
        static struct gd32_i2c_bus gd32_i2c;
        GPIO_InitPara GPIO_InitStructure;
        
        RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOB | RCC_APB2PERIPH_AF,
                                   ENABLE);
        
        /* Configure I2C_EE pins: SCL and SDA */
        GPIO_InitStructure.GPIO_Pin =  GPIO_PIN_6 | GPIO_PIN_7; 
        GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
        GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_OD;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
        
        gd32_i2c.bus_addr = PCF8563_ADDR;
        
        gd32_i2c_register(I2C1, &gd32_i2c, "i2c1");
    }
}
#endif /* RT_USING_I2C */

/**
  * @brief  Configure the nested vectored interrupt controller.
  * @param  None
  * @retval None
  */
static void NVIC_Configuration(void)
{
#ifdef  VECT_TAB_RAM
    /* Set the Vector Table base location at 0x20000000 */
    NVIC_VectTableSet(NVIC_VECTTAB_RAM, 0x0);
#else  /* VECT_TAB_FLASH  */
    /* Set the Vector Table base location at 0x08008000 */
    /* IAP program will set entry at this offset */
    NVIC_VectTableSet(NVIC_VECTTAB_FLASH, 0x8000);
#endif /* VECT_TABLE_SET */
}

/**
 * @brief  This function will initialize bwnc board.
 */
void rt_hw_board_init(void)
{
    /* NVIC Configuration */
    NVIC_Configuration();

    /* Configure the SysTick */
    SysTick_Config( SystemCoreClock / RT_TICK_PER_SECOND );

    /* configure debug print interface */
    rt_hw_usart_init();
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
    
    {
        /* configure watchdog gpio */
        extern void rt_hw_init_watchdog(void);
        rt_hw_init_watchdog();
    }

#ifdef RT_USING_SPI    
    /* configure spi interface */
    rt_hw_spi_init();

#ifdef RT_USING_LORA    
    /* hard reset sx1301 module */
    rt_hw_loragw_reset();
    
    /* configure sx1301 module */
    loragw_init(RT_LORAGW_DEVICE_NAME, "spi3_0");
#endif /* RT_USING_LORA */

#endif /* RT_USING_SPI */

#ifdef RT_USING_I2C
    /* initailize i2c interface */
    rt_hw_i2c_init();
#endif /* RT_USING_I2C */

#ifdef RT_USING_LWIP
    /* initailize eth interface, must get mac address first */
    rt_hw_gd32_eth_init();
#endif /* RT_USING_LWIP */

#ifdef RT_USING_LED
    /* initailize front led module */
    rt_hw_led_init();
#endif /* RT_USING_LED */

}
 
/* ****************************** end of file ****************************** */
