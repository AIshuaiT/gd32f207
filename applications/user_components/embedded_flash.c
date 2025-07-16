/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : embedded_flash.c
 * Arthor    : Test
 * Date      : May 3rd, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-03      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "gd32f20x_fmc.h"
#include "embedded_flash.h"

#include <stdlib.h>

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define PAGE_SIZE                   ((uint16_t)2048)  /* Page size = 2KByte */
#define EMBEDFLASH_START_ADDRESS    ((uint32_t)0x08007800)//30K
#define	DEVICE_PARAMS_ADDRESS       EMBEDFLASH_START_ADDRESS

#define DEFAULT_DEVICE_PARAMS \
        { \
        "448", \
        {0x00, 0x1e, 0x38, 0x00, 0x00, 0x00}, \
        0, \
        448, \
        }

 
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

device_params_t wnc_device;
 
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
 * @brief  get device parameters save on embedded flash
 */
void get_device_params(void)
{
    int i;
    uint8_t * ptr;
    
    ptr = (uint8_t *)&wnc_device;

    for(i = 0; i < sizeof(wnc_device); i++)
    {
        *(ptr + i) = (*(__IO uint8_t *)(DEVICE_PARAMS_ADDRESS + i));
    }
    
    if(wnc_device.id == 0xffffffff)
    {
        wnc_device.id = strtol((char *)wnc_device.id_str, RT_NULL, 0);
    }
}

/**
 * @brief  save device parameters to embedded flash
 * @param  data - pointer to data need save
 * @retval RT_EOK, other means failed
 */
rt_err_t save_device_params(const uint16_t * data)
{
    int i;
    
    /* device parameters save on bank 1 */
    FMC_UnlockB1();
    
    if(FMC_ErasePage(DEVICE_PARAMS_ADDRESS) != FMC_READY)
    {
        FMC_LockB1();
        return RT_ERROR;
    }

    for(i = 0; i < sizeof(wnc_device)/2; i++)
    {
        if(FMC_ProgramHalfWord((DEVICE_PARAMS_ADDRESS + i * 2), (*(data + i))) != FMC_READY)
        {
            FMC_LockB1();
            return RT_ERROR;
        }
    }
    
    FMC_LockB1();
    
    return RT_EOK;
}

/**
 * @brief  initailize device parameters to default value
 * @retval RT_EOK, other means failed
 */
rt_err_t init_device_param(void)
{
    device_params_t default_params = DEFAULT_DEVICE_PARAMS;
    rt_memcpy(&wnc_device, &default_params, sizeof(default_params));
    return (save_device_params((uint16_t *)&wnc_device));
}
 
/* ****************************** end of file ****************************** */
