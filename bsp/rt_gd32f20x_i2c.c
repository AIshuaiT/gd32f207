/**
 * @LOG:
 * May 5th, 2017. By Test.
 * Modify from rt_gd32f20x_spi.*. For now only use for pcf8536. 7bit address.
 * Only support i2c master mode because the driver write in i2c_core.c
 * and i2c_dev.c. 
 */

#include "rt_gd32f20x_i2c.h"

#define I2C_OPT_TIMEOUT         (0xffff)
#define I2C_Speed               (100000)


static rt_size_t master_xfer(struct rt_i2c_bus_device *bus,
                             struct rt_i2c_msg msgs[],
                             rt_uint32_t num);
static rt_size_t slave_xfer(struct rt_i2c_bus_device *bus,
                            struct rt_i2c_msg msgs[],
                            rt_uint32_t num);
static rt_err_t i2c_bus_control(struct rt_i2c_bus_device *bus,
                                rt_uint32_t,
                                rt_uint32_t);
                            
static struct rt_i2c_bus_device_ops gd32_i2c_ops =
{
    master_xfer,
    slave_xfer,
    i2c_bus_control,
};

static rt_size_t i2c_buffer_write(struct gd32_i2c_bus *bus, uint16_t addr, uint8_t *buf, uint16_t len)
{
    uint32_t timeout;
    rt_size_t ret = len;
    
    /* reset time counter */
    timeout = 0;
    /* wait i2c free */
    do
    {
        ++timeout;
    } while(I2C_GetBitState(bus->I2C, I2C_FLAG_I2CBSY) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    
    /* Send START condition */
    I2C_StartOnBus_Enable(bus->I2C, ENABLE);
    
    /* reset time counter */
    timeout = 0;
    /* wait master mode sbsend */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_SBSEND) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    
    /* send 7bit address */
    I2C_AddressingDevice_7bit(bus->I2C, bus->bus_addr, I2C_DIRECTION_TRANSMITTER);
    
    /* reset time counter */
    timeout = 0;
    /* wait addsend & transmit */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_TRANSMITTER_ADDSEND | I2C_PROGRAMMINGMODE_MASTER_BYTE_TRANSMITTING) && 
            (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    
    /* send register offset */
    I2C_SendData(bus->I2C, addr);
    
    /* reset time counter */
    timeout = 0;
    /* wait transmit */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_BYTE_TRANSMITTED) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }

    /* While there is data to be written */
    while(len--)
    {
        /* Send the current byte */
        I2C_SendData(bus->I2C, *buf); 

        /* Point to the next byte to be written */
        buf++;
        
        /* reset time counter */
        timeout = 0;
        /* wait transmit */
        do
        {
            ++timeout;
        } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_BYTE_TRANSMITTED) && (timeout < I2C_OPT_TIMEOUT));
        /* return error when timeout */
        if(timeout == I2C_OPT_TIMEOUT)
        {
            return 0;
        }
    }

    /* Send STOP condition */
    I2C_StopOnBus_Enable(bus->I2C, ENABLE);
    
    return ret;
}

static rt_size_t i2c_buffer_read(struct gd32_i2c_bus *bus, uint16_t addr, uint8_t *buf, uint16_t len)
{
    uint32_t timeout;
    rt_size_t ret = len;
    
    /* reset time counter */
    timeout = 0;
    /* wait i2c free */
    do
    {
        ++timeout;
    } while(I2C_GetBitState(bus->I2C, I2C_FLAG_I2CBSY) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }

    /* Send START condition */
    I2C_StartOnBus_Enable(bus->I2C, ENABLE);
    
    /* reset time counter */
    timeout = 0;
    /* wait master mode sbsend */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_SBSEND) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    
    /* send 7bit address */
    I2C_AddressingDevice_7bit(bus->I2C, bus->bus_addr, I2C_DIRECTION_TRANSMITTER);
    
    /* reset time counter */
    timeout = 0;
    /* wait addsend & transmit */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_TRANSMITTER_ADDSEND | I2C_PROGRAMMINGMODE_MASTER_BYTE_TRANSMITTING) && 
            (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    /* send register offset */
    I2C_SendData(bus->I2C, addr);
    
    /* reset time counter */
    timeout = 0;
    /* wait transmit */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_BYTE_TRANSMITTED) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }

    /* Send START condition again */
    I2C_StartOnBus_Enable(bus->I2C, ENABLE);

    /* reset time counter */
    timeout = 0;
    /* wait master mode sbsend */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_SBSEND) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    
    /* send 7bit address, receiver mode */
    I2C_AddressingDevice_7bit(bus->I2C, bus->bus_addr, I2C_DIRECTION_RECEIVER);
    
    /* reset time counter */
    timeout = 0;
    /* wait master mode sbsend */
    do
    {
        ++timeout;
    } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_RECEIVER_ADDSEND) && (timeout < I2C_OPT_TIMEOUT));
    /* return error when timeout */
    if(timeout == I2C_OPT_TIMEOUT)
    {
        return 0;
    }
    
    /* While there is data to be read */
    while(len)
    {
        /* last byte */
        if(len == 1)
        {
            /* Disable Acknowledgement */
            I2C_Acknowledge_Enable(bus->I2C, DISABLE);
            
            /* Send STOP condition */
            I2C_StopOnBus_Enable(bus->I2C, ENABLE);           
        }
        
        /* reset time counter */
        timeout = 0;
        /* wait master mode sbsend */
        do
        {
            ++timeout;
        } while(!I2C_StateDetect(bus->I2C, I2C_PROGRAMMINGMODE_MASTER_BYTE_RECEIVED) && (timeout < I2C_OPT_TIMEOUT));
        /* return error when timeout */
        if(timeout == I2C_OPT_TIMEOUT)
        {
            return 0;
        }
        
        /* Read a byte */
        *buf = I2C_ReceiveData(bus->I2C);
        buf++;
        
        /* Decrement the read bytes counter */
        len--;        
    }
    
    /* Enable Acknowledgement to be ready for another reception */
    I2C_Acknowledge_Enable(bus->I2C, ENABLE);
    
    return ret;
}

static rt_size_t master_xfer(
    struct rt_i2c_bus_device *bus,
    struct rt_i2c_msg msgs[],
    rt_uint32_t num
)
{
    rt_size_t ret;
    struct gd32_i2c_bus *i2c_bus = (struct gd32_i2c_bus*)bus;
        
    int i;
    
    rt_enter_critical();
    
    for(i = 0; i < num; i++)
    {
        ret = (msgs[i].flags & RT_I2C_RD) ? \
              i2c_buffer_read(i2c_bus, msgs[i].addr, msgs[i].buf, msgs[i].len) : \
              i2c_buffer_write(i2c_bus, msgs[i].addr, msgs[i].buf, msgs[i].len);
    }
    
    rt_exit_critical();

    return ret;
};

/* not support, not use */
static rt_size_t slave_xfer(
    struct rt_i2c_bus_device *bus,
    struct rt_i2c_msg msgs[],
    rt_uint32_t num
)
{
    return 0;
}

/* not support, not use */
static rt_err_t i2c_bus_control(
    struct rt_i2c_bus_device *bus,
    rt_uint32_t a,      /* don't know how to use */
    rt_uint32_t b
)
{
    return 0;
}

/** \brief init and register gd32 i2c bus.
 *
 * \param I2C: gd32 I2C, e.g: I2C1,I2C2.
 * \param gd32_i2c: gd32 i2c bus struct.
 * \param i2c_bus_name: i2c bus name, e.g: "i2c1"
 * \return
 *
 */
rt_err_t gd32_i2c_register(I2C_TypeDef * I2C,
                           struct gd32_i2c_bus * gd32_i2c,
                           const char * i2c_bus_name)
{
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_AF, ENABLE);

    if(I2C == I2C1)
    {
        I2C_InitPara I2C_InitStructure;
        
    	gd32_i2c->I2C = I2C1;
        RCC_APB1PeriphClock_Enable(RCC_APB1PERIPH_I2C1, ENABLE);
        
        I2C_DeInit(gd32_i2c->I2C);        
        /* I2C configuration */
        I2C_InitStructure.I2C_Protocol = I2C_PROTOCOL_I2C;
        I2C_InitStructure.I2C_DutyCycle = I2C_DUTYCYCLE_2;
        I2C_InitStructure.I2C_DeviceAddress = gd32_i2c->bus_addr;
        I2C_InitStructure.I2C_AddressingMode = I2C_ADDRESSING_MODE_7BIT;
        I2C_InitStructure.I2C_BitRate = I2C_Speed;

        /* I2C Peripheral Enable */
        I2C_Enable(gd32_i2c->I2C, ENABLE);
        /* Apply I2C configuration after enabling it */
        I2C_Init(gd32_i2c->I2C, &I2C_InitStructure);
        I2C_Acknowledge_Enable(gd32_i2c->I2C, ENABLE);
    }
    else
    {
        return RT_ENOSYS;
    }

    gd32_i2c->parent.ops = &gd32_i2c_ops;
    
    return rt_i2c_bus_device_register(&gd32_i2c->parent, i2c_bus_name);
}
