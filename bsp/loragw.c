/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : loragw.c
 * Arthor    : Test
 * Date      : Apr 6th, 2017
 *
 * Driver of sx1301 module.
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-06      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "loragw.h"
#include "gd32f20x.h"
#include "loragw_aux.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

//#define LORAGW_TRACE

#ifdef LORAGW_TRACE
#define LORAGW_DEBUG(...)         rt_kprintf("[LORAGW] %d ", rt_tick_get()); rt_kprintf(__VA_ARGS__);
#else
#define LORAGW_DEBUG(...)
#endif /* #ifdef LORAGW_TRACE */

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

static struct loragw_device _loragw_dev;
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

/* RT-Thread Device Driver Interface */
static rt_err_t  rt_loragw_init  (rt_device_t dev);
static rt_err_t  rt_loragw_open  (rt_device_t dev, rt_uint16_t oflag);
static rt_err_t  rt_loragw_close (rt_device_t dev);
static rt_size_t rt_loragw_read  (
    rt_device_t dev, 
    rt_off_t pos, 
    void* buffer, 
    rt_size_t size
);
static rt_size_t rt_loragw_write (
    rt_device_t dev, 
    rt_off_t pos, 
    const void* buffer, 
    rt_size_t size
);
/* RT-Thread Device Driver Interface End */

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

/* RT-Thread Device Driver Interface */
static rt_err_t rt_loragw_init(rt_device_t dev)
{
    struct loragw_device * loragw;
    
    RT_ASSERT(dev != RT_NULL);
    loragw = (struct loragw_device *)dev;
    RT_ASSERT(loragw->spi_device != RT_NULL);
    RT_ASSERT(loragw->spi_device->bus != RT_NULL);
    
    /* config spi */
    {
        struct rt_spi_configuration cfg;
        cfg.data_width = 8;
        cfg.mode = RT_SPI_MODE_0 | RT_SPI_MSB; /* SPI Compatible Modes 0 */
        cfg.max_hz = 8000000; /* 8Mbit/s */ /* sx1301 module needs lower than 10Mbit/s */
        rt_spi_configure(loragw->spi_device, &cfg);
    } /* config spi end */
    
    return RT_EOK;
}

static rt_err_t rt_loragw_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_loragw_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_size_t rt_loragw_read(
    rt_device_t dev, 
    rt_off_t pos, 
    void* buffer, 
    rt_size_t size
)
{
    rt_err_t result;
    struct loragw_device * loragw;
    struct rt_spi_message message;
    
    /* check params */
    RT_ASSERT(dev != RT_NULL);
    if (size == 0) return 0;
    loragw = (struct loragw_device *)dev;
    RT_ASSERT(loragw->spi_device != RT_NULL);
    RT_ASSERT(loragw->spi_device->bus != RT_NULL);
    
    result = rt_mutex_take(&(loragw->spi_device->bus->lock), RT_WAITING_FOREVER);
    if (result == RT_EOK)
    {
        if (loragw->spi_device->bus->owner != loragw->spi_device)
        {
            /* not the same owner as current, re-configure SPI bus */
            result = loragw->spi_device->bus->ops->configure(loragw->spi_device, &loragw->spi_device->config);
            if (result == RT_EOK)
            {
                /* set SPI bus owner */
                loragw->spi_device->bus->owner = loragw->spi_device;
            }
            else
            {
                /* configure SPI bus failed */
                rt_mutex_release(&(loragw->spi_device->bus->lock));
                return 0;
            }
        }

        /* send address first */
        message.send_buf = &pos;
        message.recv_buf = RT_NULL;
        message.length = 1;
        message.cs_take = 1;
        message.cs_release = 0;
        loragw->spi_device->bus->ops->xfer(loragw->spi_device, &message);
        
        /* initial message */
        message.send_buf = RT_NULL;
        message.recv_buf = buffer;
        message.length = size;
        message.cs_take = 0;
        message.cs_release = 1;

        /* transfer message */
        loragw->spi_device->bus->ops->xfer(loragw->spi_device, &message);
    }
    else
    {
        return 0;
    }
    
    rt_mutex_release(&(loragw->spi_device->bus->lock));
    return size;
}

static rt_size_t rt_loragw_write (
    rt_device_t dev, 
    rt_off_t pos, 
    const void* buffer, 
    rt_size_t size
)
{
    rt_err_t result;
    struct loragw_device * loragw;
    struct rt_spi_message message;
    
    /* check params */
    RT_ASSERT(dev != RT_NULL);
    if (size == 0) return 0;
    loragw = (struct loragw_device *)dev;
    RT_ASSERT(loragw->spi_device != RT_NULL);
    RT_ASSERT(loragw->spi_device->bus != RT_NULL);
    
    result = rt_mutex_take(&(loragw->spi_device->bus->lock), RT_WAITING_FOREVER);
    if (result == RT_EOK)
    {
        if (loragw->spi_device->bus->owner != loragw->spi_device)
        {
            /* not the same owner as current, re-configure SPI bus */
            result = loragw->spi_device->bus->ops->configure(loragw->spi_device, &loragw->spi_device->config);
            if (result == RT_EOK)
            {
                /* set SPI bus owner */
                loragw->spi_device->bus->owner = loragw->spi_device;
            }
            else
            {
                /* configure SPI bus failed */
                rt_mutex_release(&(loragw->spi_device->bus->lock));
                return 0;
            }
        }

        /* send address first */
        message.send_buf = &pos;
        message.recv_buf = RT_NULL;
        message.length = 1;
        message.cs_take = 1;
        message.cs_release = 0;
        loragw->spi_device->bus->ops->xfer(loragw->spi_device, &message);
        
        /* initial message */
        message.send_buf = buffer;
        message.recv_buf = RT_NULL;
        message.length = size;
        message.cs_take = 0;
        message.cs_release = 1;

        /* transfer message */
        loragw->spi_device->bus->ops->xfer(loragw->spi_device, &message);
    }
    else
    {
        return 0;
    }
    
    rt_mutex_release(&(loragw->spi_device->bus->lock));
    return size;
}

/* RT-Thread Device Driver Interface End */    

/**
 * @brief  initialize sx1301 module. 
 * @param  loragw_device_name: name of loragw device, length should less than RT_NAME_MAX.
 * @param  spi_device_name: name of spi device, should be already registered to system.
 * @retval RT_EOK when success, others when got error.
 */
rt_err_t loragw_init(
    const char *loragw_device_name, 
    const char *spi_device_name
)
{
    rt_err_t result = RT_EOK;    
    struct rt_spi_device * spi_device;

    spi_device = (struct rt_spi_device *)rt_device_find(spi_device_name);
    if(spi_device == RT_NULL)
    {
        LORAGW_DEBUG("spi device %s not found!\r\n", spi_device_name);
        return -RT_ENOSYS;
    }
    rt_memset(&_loragw_dev, 0, sizeof(_loragw_dev));
    
    /* register loragw device */
    _loragw_dev.spi_device = spi_device;
    
    _loragw_dev.parent.type         = RT_Device_Class_SPIDevice;
    
    /* register generate interface */
    _loragw_dev.parent.init         = rt_loragw_init;
    _loragw_dev.parent.open         = rt_loragw_open;
    _loragw_dev.parent.close        = rt_loragw_close;
    _loragw_dev.parent.read         = rt_loragw_read;
    _loragw_dev.parent.write        = rt_loragw_write;
    
    /* no control interface */
    _loragw_dev.parent.control      = RT_NULL;
    
    /* no private, no callback */
    _loragw_dev.parent.user_data    = RT_NULL;
    _loragw_dev.parent.rx_indicate  = RT_NULL;
    _loragw_dev.parent.tx_complete  = RT_NULL;

    result = rt_device_register(
                &_loragw_dev.parent, 
                loragw_device_name,
                RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE);
    
    return result;
}

/**
 * @brief  hardware reset sx1301 module
 */ 
void rt_hw_loragw_reset(void)
{
    GPIO_InitPara GPIO_InitStructure;

    /* Enable GPIO clock */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_PIN_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_SPEED_50MHZ;
    GPIO_InitStructure.GPIO_Mode  = GPIO_MODE_OUT_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_SetBits(GPIOB, GPIO_PIN_5);
	wait_ms(100);
    GPIO_ResetBits(GPIOB, GPIO_PIN_5);
    wait_ms(400);
}

 
/* ****************************** end of file ****************************** */
