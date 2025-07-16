#ifndef GD32_I2C_H_INCLUDED
#define GD32_I2C_H_INCLUDED

#include <rtdevice.h>

#include "gd32f20x.h"
#include "gd32f20x_i2c.h"

#include "board.h"

struct gd32_i2c_bus
{
    struct rt_i2c_bus_device parent;
    I2C_TypeDef * I2C;
    uint8_t bus_addr;
};

/* public function list */
rt_err_t gd32_i2c_register(I2C_TypeDef * I2C,
                           struct gd32_i2c_bus * gd32_i2c,
                           const char * i2c_bus_name);

#endif // GD32_I2C_H_INCLUDED
