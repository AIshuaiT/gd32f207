/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_led.c
 * Arthor    : Test
 * Date      : May 22th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-22      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "thread_led.h"
#include "gd32f20x.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

/* timers */
#define RT_TIMER_NAME_LED_TEST      "led_test"
#define RT_TIMER_TIMEOUT_LED_TEST   ((500 * RT_TICK_PER_SECOND)/1000) /* 500ms */

#define TIMER_CLK_FREQ      (1000000)               /* 1 MHz */
#define TIMER_PERIOD        (TIMER_CLK_FREQ/8/50)   /* 8 lines, 50 Hz */

const GPIO_TypeDef* column_gpio_port[8] = 
{
	GPIOE,
	GPIOE,
	GPIOE,
	GPIOE,
	GPIOE,
	GPIOE,
	GPIOE,
	GPIOE,
};

const uint16_t column_gpio_pin[8] =
{
	GPIO_PIN_0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3,
	GPIO_PIN_10,
	GPIO_PIN_9,
	GPIO_PIN_8,
	GPIO_PIN_7,
};

#define SET_COLUMN_ON(x)   GPIO_ResetBits((GPIO_TypeDef*)column_gpio_port[x], column_gpio_pin[x])
#define SET_COLUMN_OFF(x)  GPIO_SetBits((GPIO_TypeDef*)column_gpio_port[x], column_gpio_pin[x])

const GPIO_TypeDef* row_gpio_port[8] =
{
	GPIOE,
	GPIOC,
	GPIOC,
	GPIOC,
	GPIOE,
	GPIOE,
	GPIOE,
	GPIOE,
};

const uint32_t row_gpio_pin[8] =
{
	GPIO_PIN_6,
	GPIO_PIN_13,
	GPIO_PIN_14,
	GPIO_PIN_15,
	GPIO_PIN_14,
	GPIO_PIN_13,
	GPIO_PIN_12,
	GPIO_PIN_11,
};

#define SET_ROW_ON(x)      GPIO_ResetBits((GPIO_TypeDef*)row_gpio_port[x], row_gpio_pin[x]) 
#define SET_ROW_OFF(x)	   GPIO_SetBits((GPIO_TypeDef*)row_gpio_port[x], row_gpio_pin[x])
 
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

rt_thread_t tid_led = RT_NULL;
rt_mq_t     mq_led  = RT_NULL;
 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static rt_uint8_t dot_buf[8];       /* 1 byte for a line, 1 bit for a light */
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static void GPIO_Configuration  (void);
static void TIM2_Configuration  (void);
static void NVIC_Configuration  (void);

static void led_init            (void);
static void led_test_fresh_buf  (void);
 
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
  * @brief  Configure the GPIOs.
  * @param  None
  * @retval None
  */ 
static void GPIO_Configuration(void)
{
    GPIO_InitPara GPIO_InitStructure;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOC | RCC_APB2PERIPH_GPIOE,
                               ENABLE);
    
    GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_OUT_PP;
    
    GPIO_InitStructure.GPIO_Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  |
                                  GPIO_PIN_6  | GPIO_PIN_7  | GPIO_PIN_8  | GPIO_PIN_9  | 
                                  GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | 
                                  GPIO_PIN_14;
    GPIO_Init(GPIOE, &GPIO_InitStructure);
}

/**
 * @brief  Configure the Timer2
 */
static void TIM2_Configuration(void)
{
    TIMER_BaseInitPara  TIM_TimeBaseStructure;
    RCC_ClocksPara      RCC_Clocks;
    uint16_t            Prescaler;

    RCC_APB1PeriphClock_Enable(RCC_APB1PERIPH_TIMER2, ENABLE);

    RCC_GetClocksFreq(&RCC_Clocks);
    Prescaler = RCC_Clocks.CK_SYS_Frequency / 1000000 - 1;     /* 1MHz */

    TIMER_DeInit(TIMER2);
    
    TIM_TimeBaseStructure.TIMER_Period = TIMER_PERIOD;
    TIM_TimeBaseStructure.TIMER_Prescaler = Prescaler;     
    TIM_TimeBaseStructure.TIMER_ClockDivision = 0;    
    TIM_TimeBaseStructure.TIMER_CounterMode = TIMER_COUNTER_UP;

    TIMER_BaseInit(TIMER2, &TIM_TimeBaseStructure);
    /* Prescaler configuration */
    TIMER_PrescalerConfig(TIMER2, Prescaler, TIMER_PSC_RELOAD_UPDATE);
    TIMER_CARLPreloadConfig(TIMER2, DISABLE);
    TIMER_InternalClockConfig(TIMER2);
    TIMER_ClearIntBitState(TIMER2, TIMER_INT_UPDATE);
    /* TIM IT enable */
    TIMER_INTConfig(TIMER2, TIMER_INT_UPDATE, ENABLE);
    TIMER_Enable(TIMER2, ENABLE);
}

/**
  * @brief  Configure the nested vectored interrupt controller.
  * @param  None
  * @retval None
  */
static void NVIC_Configuration(void)
{
    NVIC_InitPara NVIC_InitStructure;

    /* Enable the Eth Interrupt */
    NVIC_InitStructure.NVIC_IRQ = TIMER2_IRQn;
    NVIC_InitStructure.NVIC_IRQPreemptPriority = 0;
    NVIC_InitStructure.NVIC_IRQSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&NVIC_InitStructure);    
}

/**
 * @brief  timer2 interrupt handler
 */
void TIMER2_IRQHandler(void)
{
    stu_led_msg msg;
    
	TIMER_INTConfig(TIMER2, TIMER_INT_UPDATE, DISABLE);

    if(mq_led != RT_NULL)
    {
        msg.type = MSG_LED_REFRESH;
        rt_mq_send(mq_led, &msg, sizeof(msg));
    }

	TIMER_ClearIntBitState(TIMER2, TIMER_INT_UPDATE);
	TIMER_INTConfig(TIMER2, TIMER_INT_UPDATE, ENABLE);
}

/**
 * @brief  rt-thread timer callback functions
 * @param  parameter: rt-thread timer callback parameter
 */
static void callback_timer_led_test(void* parameter)
{
    led_test_fresh_buf();
}

/**
 * @brief  initailize led module gpio and timer to refresh led
 */
void rt_hw_led_init(void)
{
    TIM2_Configuration();
    GPIO_Configuration();
    NVIC_Configuration();
}

/**
 * @brief  initailize led buffer
 */
static void led_init(void)
{
	rt_memset((void*)dot_buf, 0, sizeof(dot_buf));

	dot_buf[7] |= 0x05;
}

/**
 * @brief  refresh led module
 */
static void refresh_led(void)
{
	static uint8_t row = 0;
	int i;

	for(i = 0; i < 8; i++)
	{
		(row == i) ? SET_ROW_ON(i) : SET_ROW_OFF(i);
	}

	for(i = 0; i < 8; i++)
	{
		(dot_buf[row] & (0x80 >> i)) ? SET_COLUMN_ON(i) : SET_COLUMN_OFF(i);
	}	

    row++;
	if(row > 7)
    {
		row = 0;
    }
}

/**
 * @brief refresh buffer when led test
 */
static void led_test_fresh_buf(void)
{
	static uint8_t direction = 0; /* 0: column by column, 1: line by line */
    
	if(direction)
	{
        static uint8_t line_num  = 0;
        
		rt_memset((void*)dot_buf, 0, sizeof(dot_buf));
		dot_buf[line_num++] = 0xff;
		if(line_num > 7)
		{
			line_num = 0;
			direction = !direction;
		}
	}
	else
	{
        static uint8_t value     = 0x80;
        
		rt_memset((void*)dot_buf, value, sizeof(dot_buf));
		value >>= 1;
		if(!value)
		{
			value = 0x80;
			direction = !direction;
		}
	}
}

/**
 * @brief  led refresh thread entry.
 * @param  parameter: rt-thread param.
 */
void thread_led(void* parameter)
{
    stu_led_msg     msg;
    rt_timer_t      timer_led_test;

    /* initailize timers */
    timer_led_test = rt_timer_create(RT_TIMER_NAME_LED_TEST,
                                     callback_timer_led_test,
                                     RT_NULL,
                                     RT_TIMER_TIMEOUT_LED_TEST,
                                     RT_TIMER_FLAG_PERIODIC);
    RT_ASSERT(timer_led_test != RT_NULL);
    
    led_init();
    
    while(1)
    {
        if(rt_mq_recv(mq_led, &msg, sizeof(msg), RT_WAITING_FOREVER) == RT_EOK)
        {
            switch(msg.type)
            {
            case MSG_LED_REFRESH:
            {
                refresh_led();
                break;
            }
            case MSG_LED_SET_LED:
            {
                enum led_state  state   = (enum led_state)msg.data[0];
                rt_uint8_t      row     = msg.data[1];
                rt_uint8_t      column  = msg.data[2];
                
                if(state == OFF)
                {
                    dot_buf[row] &= ~(0x80 >> column);
                }
                else
                {
                    dot_buf[row] |= 0x80 >> column;
                }
                break;
            }
            case MSG_LED_TEST:
            {
                static rt_uint8_t test_mode = 0;
                if(test_mode == 0)
                {
                    test_mode = 1;
                    rt_timer_start(timer_led_test);
                }
                else
                {
                    test_mode = 0;
                    rt_timer_stop(timer_led_test);
                    led_init();
                }
                break;
            }
            default:
            {
                /* wrong message */
                break;
            }
            }
        }
    }
}

/**
 * @brief  set one led on or off
 * @param  state: ON or OFF state definition in led_state
 * @param  row: row from top
 * @param  column: column from left
 */
void set_led(enum led_state state, rt_uint8_t row, rt_uint8_t column)
{
    stu_led_msg msg;
    
	if(row > 7 || column > 7)
    {
		return;
    }

    if(mq_led != RT_NULL)
    {
        msg.type = MSG_LED_SET_LED;
        msg.data[0] = state;
        msg.data[1] = row;
        msg.data[2] = column;
        rt_mq_send(mq_led, &msg, sizeof(msg));
    }
}

/* ****************************** end of file ****************************** */
