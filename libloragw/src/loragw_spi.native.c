/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    Host specific functions to address the LoRa concentrator registers through
    a SPI interface.
    Single-byte read/write and burst read/write.
    Does not handle pagination.
    Could be used with multiple SPI ports in parallel (explicit file descriptor)

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Sylvain Miermont
*/

/**
 * CHANGELOG
 ******************************************************************************
 * 2017/2/10 by sue
 * Change platform from linux to uc/OS on cortex-M3(stm32xx or gd32xx).
 *
 *
 ******************************************************************************
 */

/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include "loragw.h"

#include "gd32f20x.h" 
#include "loragw_spi.h"
#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_SPI == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_SPI_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)                if(a==NULL){return LGW_SPI_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define READ_ACCESS     0x00
#define WRITE_ACCESS    0x80

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/* SPI initialization and configuration */
int lgw_spi_open(void **spi_target_ptr) {

    int *spi_device = NULL;
    rt_device_t dev_id;

    /* allocate memory for the device descriptor */
    spi_device = rt_malloc(sizeof(int));
    if (spi_device == NULL) {
        DEBUG_MSG("ERROR: MALLOC FAIL\n");
        return LGW_SPI_ERROR;
    }
    
    if ((dev_id = rt_device_find(RT_LORAGW_DEVICE_NAME)) == RT_NULL)
    {
        /* no this device */
        free(spi_device);
        return LGW_SPI_ERROR;
    }
    
    if (rt_device_open(dev_id, RT_DEVICE_OFLAG_RDWR) != RT_EOK)
    {
        /* cannot open device */
        free(spi_device);
        return LGW_SPI_ERROR;
    }

    *spi_device = (int)dev_id;
    *spi_target_ptr = (void *)spi_device;
    DEBUG_MSG("Note: SPI port opened and configured ok\n");

    return LGW_SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* SPI release */
int lgw_spi_close(void *spi_target) {

    rt_device_t dev_id;
    rt_err_t result;
    
    /* check input variables */
    CHECK_NULL(spi_target);
    
    /* close file & deallocate file descriptor */
    dev_id = *(rt_device_t *)spi_target; /* must check that spi_target is not null beforehand */
    result = rt_device_close(dev_id);
    free(spi_target);

    /* determine return code */
    if (result != RT_EOK) {
        DEBUG_MSG("ERROR: SPI PORT FAILED TO CLOSE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI port closed\n");
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
int lgw_spi_w(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t data) {

    /* operate as one byte burst write */
	return lgw_spi_wb(spi_target, spi_mux_mode, spi_mux_target, address, &data, 1);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
int lgw_spi_r(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data) {

    /* operate as one byte burst read */
	return lgw_spi_rb(spi_target, spi_mux_mode, spi_mux_target, address, data, 1);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) write */
int lgw_spi_wb(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data, uint16_t size) {

    rt_device_t dev_id;
    int size_to_do, chunk_size, offset;
    int byte_transfered = 0;
    int i;
    
    /* check input parameters */
    CHECK_NULL(spi_target);
    if ((address & 0x80) != 0) {
        DEBUG_MSG("WARNING: SPI address > 127\n");
    }
    CHECK_NULL(data);
    if (size == 0) {
        DEBUG_MSG("ERROR: BURST OF NULL LENGTH\n");
        return LGW_SPI_ERROR;
    }
    
    dev_id = *(rt_device_t *)spi_target; /* must check that spi_target is not null beforehand */

    size_to_do = size; 
    for (i=0; size_to_do > 0; ++i) {
        chunk_size = (size_to_do < LGW_BURST_CHUNK) ? size_to_do : LGW_BURST_CHUNK;
        offset = i * LGW_BURST_CHUNK;
        byte_transfered += rt_device_write(dev_id, (WRITE_ACCESS | (address & 0x7F)), (data + offset), chunk_size);
        DEBUG_PRINTF("BURST WRITE: to trans %d # chunk %d # transferred %d \n", size_to_do, chunk_size, byte_transfered);
        size_to_do -= chunk_size; /* subtract the quantity of data already transferred */
    }
    
    /* determine return code */
    if (byte_transfered != size) {
        DEBUG_MSG("ERROR: SPI BURST WRITE FAILURE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI burst write success\n");
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) read */
int lgw_spi_rb(void *spi_target, uint8_t spi_mux_mode, uint8_t spi_mux_target, uint8_t address, uint8_t *data, uint16_t size) {

    rt_device_t dev_id;
    int size_to_do, chunk_size, offset;
    int byte_transfered = 0;
    int i;
    
    /* check input parameters */
    CHECK_NULL(spi_target);
    if ((address & 0x80) != 0) {
        DEBUG_MSG("WARNING: SPI address > 127\n");
    }
    CHECK_NULL(data);
    if (size == 0) {
        DEBUG_MSG("ERROR: BURST OF NULL LENGTH\n");
        return LGW_SPI_ERROR;
    }
    
    dev_id = *(rt_device_t *)spi_target; /* must check that spi_target is not null beforehand */

    size_to_do = size;    
    for (i=0; size_to_do > 0; ++i) {
        chunk_size = (size_to_do < LGW_BURST_CHUNK) ? size_to_do : LGW_BURST_CHUNK;
        offset = i * LGW_BURST_CHUNK;
        byte_transfered += rt_device_read(dev_id, (READ_ACCESS | (address & 0x7F)), (data + offset), chunk_size);
        DEBUG_PRINTF("BURST WRITE: to trans %d # chunk %d # transferred %d \n", size_to_do, chunk_size, byte_transfered);
        size_to_do -= chunk_size; /* subtract the quantity of data already transferred */
    }
    
    /* determine return code */
    if (byte_transfered != size) {
        DEBUG_MSG("ERROR: SPI BURST WRITE FAILURE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI burst write success\n");
        return LGW_SPI_SUCCESS;
    }
}

/* --- EOF ------------------------------------------------------------------ */
