/*
 * File      : usart.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006-2013, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 * 2010-03-29     Bernard      remove interrupt Tx and DMA Rx mode
 * 2013-05-13     aozima       update for kehong-lingtai.
 * 2015-01-31     armink       make sure the serial transmit complete in putc()
 */

#include "gd32f20x.h"
#include "usart.h"
#include "board.h"
#include <rtdevice.h>

/* USART1 */
#define UART1_GPIO_TX        GPIO_PIN_9
#define UART1_GPIO_RX        GPIO_PIN_10
#define UART1_GPIO           GPIOA

/* gd32 uart driver */
struct gd32_uart
{
    USART_TypeDef* uart_device;
    IRQn_Type irq;
};

static rt_err_t gd32_configure(struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct gd32_uart* uart;
    USART_InitPara USART_InitStructure;

    RT_ASSERT(serial != RT_NULL);
    RT_ASSERT(cfg != RT_NULL);

    uart = (struct gd32_uart *)serial->parent.user_data;

    USART_InitStructure.USART_BRR = cfg->baud_rate;

    if (cfg->data_bits == DATA_BITS_8){
        USART_InitStructure.USART_WL = USART_WL_8B;
    } else if (cfg->data_bits == DATA_BITS_9) {
        USART_InitStructure.USART_WL = USART_WL_9B;
    }

    if (cfg->stop_bits == STOP_BITS_1){
        USART_InitStructure.USART_STBits = USART_STBITS_1;
    } else if (cfg->stop_bits == STOP_BITS_2){
        USART_InitStructure.USART_STBits = USART_STBITS_2;
    }

    if (cfg->parity == PARITY_NONE){
        USART_InitStructure.USART_Parity = USART_PARITY_RESET;
    } else if (cfg->parity == PARITY_ODD) {
        USART_InitStructure.USART_Parity = USART_PARITY_SETODD;
    } else if (cfg->parity == PARITY_EVEN) {
        USART_InitStructure.USART_Parity = USART_PARITY_SETEVEN;
    }

    USART_InitStructure.USART_HardwareFlowControl = USART_HARDWAREFLOWCONTROL_NONE;
    USART_InitStructure.USART_RxorTx = USART_RXORTX_RX | USART_RXORTX_TX;
    USART_Init(uart->uart_device, &USART_InitStructure);

    /* Enable USART */
    USART_Enable(uart->uart_device, ENABLE);

    return RT_EOK;
}

static rt_err_t gd32_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct gd32_uart* uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct gd32_uart *)serial->parent.user_data;

    switch (cmd)
    {
        /* disable interrupt */
    case RT_DEVICE_CTRL_CLR_INT:
        /* disable rx irq */
        UART_DISABLE_IRQ(uart->irq);
        /* disable interrupt */
        USART_INT_Set(uart->uart_device, USART_INT_RBNE, DISABLE);
        break;
        /* enable interrupt */
    case RT_DEVICE_CTRL_SET_INT:
        /* enable rx irq */
        UART_ENABLE_IRQ(uart->irq);
        /* enable interrupt */
        USART_INT_Set(uart->uart_device, USART_INT_RBNE, ENABLE);
        break;
    }

    return RT_EOK;
}

static int gd32_putc(struct rt_serial_device *serial, char c)
{
    struct gd32_uart* uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct gd32_uart *)serial->parent.user_data;

    uart->uart_device->DR = c;
    while (!(uart->uart_device->STR & USART_FLAG_TC));

    return 1;
}

static int gd32_getc(struct rt_serial_device *serial)
{
    int ch;
    struct gd32_uart* uart;

    RT_ASSERT(serial != RT_NULL);
    uart = (struct gd32_uart *)serial->parent.user_data;

    ch = -1;
    if (uart->uart_device->STR & USART_FLAG_RBNE)
    {
        ch = uart->uart_device->DR & 0xff;
    }

    return ch;
}

static const struct rt_uart_ops gd32_uart_ops =
{
    gd32_configure,
    gd32_control,
    gd32_putc,
    gd32_getc,
};

#if defined(RT_USING_UART1)
/* UART1 device driver structure */
struct gd32_uart uart1 =
{
    USART1,
    USART1_IRQn,
};
struct rt_serial_device serial1;

void USART1_IRQHandler(void)
{
    struct gd32_uart* uart;

    uart = &uart1;

    /* enter interrupt */
    rt_interrupt_enter();
    if(USART_GetIntBitState(uart->uart_device, USART_INT_RBNE) != RESET)
    {
        rt_hw_serial_isr(&serial1, RT_SERIAL_EVENT_RX_IND);
        /* clear interrupt */
        USART_ClearIntBitState(uart->uart_device, USART_INT_RBNE);
    }

    if (USART_GetIntBitState(uart->uart_device, USART_INT_TBE) != RESET)
    {
        /* clear interrupt */
        USART_ClearIntBitState(uart->uart_device, USART_INT_TBE);
    }
    if (USART_GetBitState(uart->uart_device, USART_FLAG_ORE) == SET)
    {
        gd32_getc(&serial1);
    }
    /* leave interrupt */
    rt_interrupt_leave();
}
#endif /* RT_USING_UART1 */


static void RCC_Configuration(void)
{
	RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_AF, ENABLE);

#if defined(RT_USING_UART1)  
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA, ENABLE);
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_USART1, ENABLE);
#endif /* RT_USING_UART1 */
}

static void GPIO_Configuration(void)
{
    GPIO_InitPara GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;

#if defined(RT_USING_UART1)
    /* Configure USART1 Rx (PA.10) as input floating */
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = UART1_GPIO_RX;
    GPIO_Init(UART1_GPIO, &GPIO_InitStructure);

	/* Configure USART1 Tx (PA.09) as alternate function push-pull */
    GPIO_InitStructure.GPIO_Mode = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Pin = UART1_GPIO_TX;
    GPIO_Init(UART1_GPIO, &GPIO_InitStructure);
#endif /* RT_USING_UART1 */

}

static void NVIC_Configuration(struct gd32_uart* uart)
{
    NVIC_InitPara NVIC_InitStructure;

    /* Enable the USART1 Interrupt */
    NVIC_InitStructure.NVIC_IRQ = uart->irq;
    NVIC_InitStructure.NVIC_IRQPreemptPriority = 2;
    NVIC_InitStructure.NVIC_IRQSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void rt_hw_usart_init(void)
{
    struct gd32_uart* uart;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    RCC_Configuration();
    GPIO_Configuration();

#if defined(RT_USING_UART1)
    uart = &uart1;
    config.baud_rate = BAUD_RATE_115200;

    serial1.ops    = &gd32_uart_ops;
    serial1.config = config;

    NVIC_Configuration(&uart1);

    /* register UART1 device */
    rt_hw_serial_register(&serial1, "uart1",
                          RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX ,
                          uart);
#endif /* RT_USING_UART1 */
}
