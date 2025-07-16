/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : log_base.c
 * Arthor    : Test
 * Date      : May 18th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-05-18      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "log.h"
#include "external_flash.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define SECTOR_USE_FLAG     (0xa5a5)

#define sn_after(a, b)      ((rt_int16_t)(b)-(rt_int16_t)(a) < 0)  //a is later than b?
#define sn_before(a, b)     sn_after((b), (a))

/* for debug */
#define DEBUG_LOG    0    /* 1: debug open; 0: debug close */
#if DEBUG_LOG
    #define DEBUG_PRINTF            rt_kprintf
#else
    #define DEBUG_PRINTF(fmt, ...)
#endif /* DEBUG_LOG */

/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

/**
 * @brief  structure containing flash sector headers
 *
 * @NOTE   now the structure only has two member both typed rt_uint16_t,
 *         if add some member with other type, please consider align, 
 *         or add "_packed" key word
 */
struct sector_header
{
    rt_uint16_t used;                   /* sector used with SECTOR_USE_FLAG */
    rt_uint16_t sn;                     /* serial number */
};

/**
 * @brief  structure containing log file operate informations
 */
struct log_info
{
    rt_uint16_t next_sn;                /* next serial number to write */
    rt_uint32_t max_size;               /* maximun file size */
    rt_uint32_t base_addr;              /* file base address on external flash */
    rt_uint32_t sector_num;             /* max sector number */
    rt_uint32_t write_offset;           /* write offset from base_addr */
    rt_uint32_t read_offset;            /* read offset from base_addr */
};

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

extern rt_device_t      dev_ext_flash;
 
 /**
 ******************************************************************************
 *                              PRIVATE VARIABLES
 ******************************************************************************
 */

static struct log_info  log_infos[MAX_LOG_TYPE];
 
/**
 ******************************************************************************
 *                         PRIVATE FUNCTION DECLARATION
 ******************************************************************************
 */

static void     perpare_write_info  (enum log_type type);
 
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
 * @brief  find log file write offset and next serial number
 * @param  type: which file to operate, definition in log_type
 */
static void perpare_write_info(enum log_type type)
{
    int         i;
    rt_uint32_t addr;
    rt_uint16_t max;
    rt_uint8_t  tmp8;
    
    struct log_info *       p_info;
    struct sector_header    header;
    
    p_info = &log_infos[type];

    /* find write sector */
    addr   = p_info->base_addr;
    max    = 0;
    for(i = 0; i < p_info->sector_num; i++)
    {
        rt_device_read(dev_ext_flash, addr, &header, sizeof(header));
        
        if(header.used == SECTOR_USE_FLAG)
        {
            if((i == 0) || sn_after(header.sn, max))
            {
                max = header.sn;
                p_info->write_offset = i * FLASH_BYTES_PER_SECTOR;
            }
        }
        else
        {
            /* sector not used */
            break;
        }
        
        addr += FLASH_BYTES_PER_SECTOR;
    }
    
    if(i == 0)
    {
        /* no data */
        return;
    }

    p_info->next_sn = max + 1;    
    p_info->write_offset += sizeof(struct sector_header);
    
    /* find write offset in sector */
    addr = p_info->base_addr + p_info->write_offset;    
    for(i = 0; i < (FLASH_BYTES_PER_SECTOR - sizeof(struct sector_header)); i++)
    {
        rt_device_read(dev_ext_flash, addr, &tmp8, sizeof(tmp8));
        if(tmp8 == 0xff)
        {
            /* find offset */
            return;
        }
        
        p_info->write_offset++;
        addr++;
    }
    /* get here when this just write to the end of this sector */
}

/**
 * @brief  find log file read offset
 * @param  type: which file to operate, definition in log_type
 */
void prepare_read(enum log_type type)
{
    int         i;
    rt_uint32_t addr;
    rt_uint16_t min;
    
    struct log_info *       p_info;
    struct sector_header    header;
    
    p_info = &log_infos[type];
    
    /* find read sector */
    addr   = p_info->base_addr;
    for(i = 0; i < p_info->sector_num; i++)
    {
        rt_device_read(dev_ext_flash, addr, &header, sizeof(header));
        
        if(header.used == SECTOR_USE_FLAG)
        {
            if((i == 0) || sn_before(header.sn, min))
            {
                min = header.sn;
                p_info->read_offset = i * FLASH_BYTES_PER_SECTOR;
            }
        }
        else
        {
            /* sector not used */
            DEBUG_PRINTF("sector not use, i = %d\r\n", i);
            break;
        }
        
        addr += FLASH_BYTES_PER_SECTOR;
    }
    
    DEBUG_PRINTF("read offset %d\r\n", p_info->read_offset);
}

/**
 * @brief  calculate log file size
 * @param  type: which file to operate, definition in log_type
 * @retval file size
 */
rt_size_t calculate_file_size(enum log_type type)
{
    rt_size_t           size;
    rt_uint16_t         sct_num;
    struct log_info *   p_info;
    
    p_info = &log_infos[type];
    
    sct_num = (p_info->read_offset > p_info->write_offset) ?                 
	          (p_info->sector_num - 1) : (p_info->write_offset / FLASH_BYTES_PER_SECTOR);
    
    size = sct_num * (FLASH_BYTES_PER_SECTOR - sizeof(struct sector_header));
    if(p_info->write_offset % FLASH_BYTES_PER_SECTOR)
    {
        size += (p_info->write_offset % FLASH_BYTES_PER_SECTOR) - sizeof(struct sector_header);
    }
    
    return size;
}

/**
 * @brief  initailize log informations, prepare to use logs
 */
void logs_init(void)
{
    enum log_type type;
    
    rt_memset(log_infos, 0, sizeof(log_infos));
    
    /* system log */
    log_infos[SYSTEM_LOG].base_addr      = SYSTEM_LOG_BASE_ADDR;
    log_infos[SYSTEM_LOG].max_size       = SYSTEM_LOG_MAX_SIZE;
    log_infos[SYSTEM_LOG].sector_num     = SYSTEM_LOG_SCT_NUM;
    
    /* work state log */
    log_infos[WORK_STATE_LOG].base_addr  = WORK_STATE_LOG_BASE_ADDR;
    log_infos[WORK_STATE_LOG].max_size   = WORK_STATE_LOG_MAX_SIZE;
    log_infos[WORK_STATE_LOG].sector_num = WORK_STATE_LOG_SCT_NUM;

    for(type = SYSTEM_LOG; type < MAX_LOG_TYPE; type++)
    {
        perpare_write_info(type);
    }
}

/**
 * @brief  write string format log data to external flash
 * @param  buf: pointer to log string buffer
 * @param  size: bytes to write
 * @param  type: which file to operate, definition in log_type
 */
void log_write(char *buf, rt_uint32_t size, enum log_type type)
{
    rt_uint32_t addr;
    rt_uint32_t free_size;
    
    struct log_info *       p_info;
    struct sector_header    header;
    
    p_info = &log_infos[type];    
    
    while(size > 0)
    {
        /* reach new sector */
        if(p_info->write_offset % FLASH_BYTES_PER_SECTOR == 0)
        {
            /* out of limit */
            if(p_info->write_offset >= p_info->max_size)
            {                
                p_info->write_offset = 0;
            }
            
            /* prepare to use this sector */
            addr = p_info->base_addr + p_info->write_offset;
            rt_device_control(dev_ext_flash, GD_FLASH_CTRL_SCT_ERASE, (void *)addr);

            header.sn = p_info->next_sn;
            p_info->next_sn++;
            header.used = SECTOR_USE_FLAG;
            
            rt_device_write(dev_ext_flash, addr, &header, sizeof(header));
            
            p_info->write_offset += sizeof(header);
        }

        /* do not cross sector */
        free_size = FLASH_BYTES_PER_SECTOR - (p_info->write_offset % FLASH_BYTES_PER_SECTOR);
        free_size = (free_size < size) ? free_size : size;
        size     -= free_size;

        addr = p_info->base_addr + p_info->write_offset;
        rt_device_write(dev_ext_flash, addr, buf, free_size);
        
        p_info->write_offset += free_size;
        buf += free_size;
    }
}

/**
 * @brief  read string format log data to data buffer
 * @param  buf: pointer to data buffer
 * @param  buf_size: maximum size of data buffer
 * @param  type: which file to operate, definition in log_type
 * @retval bytes number read out
 */
rt_size_t log_read(char *buf, rt_uint32_t buf_size, enum log_type type)
{
    rt_uint32_t addr;
    rt_uint32_t read_size;
    rt_uint32_t tmp_size;

    struct log_info *       p_info;
    
    p_info = &log_infos[type];   
    
    read_size = 0;
    while(buf_size > 0)
    {
        /* reach new sector */
        if(p_info->read_offset % FLASH_BYTES_PER_SECTOR == 0)
        {
            /* out of limit */
            if(p_info->read_offset >= p_info->max_size)
            {                
                p_info->read_offset = 0;
            }
            p_info->read_offset += sizeof(struct sector_header);
        }

        /* read address not in current write sector */
        if((p_info->read_offset  / FLASH_BYTES_PER_SECTOR) != 
           (p_info->write_offset / FLASH_BYTES_PER_SECTOR))
		{
            tmp_size = FLASH_BYTES_PER_SECTOR - (p_info->read_offset % FLASH_BYTES_PER_SECTOR);
		}
        else
        {
            tmp_size = p_info->write_offset - p_info->read_offset;
        }

        if(tmp_size == 0)
        {
            /* no more data */
            return read_size;  
        }
        else
        {
            tmp_size = (buf_size < tmp_size) ? buf_size : tmp_size;
        }

        addr = p_info->base_addr + p_info->read_offset;
        rt_device_read(dev_ext_flash, addr, buf, tmp_size);
        
        p_info->read_offset += tmp_size;
        read_size           += tmp_size;
        buf                 += tmp_size;
        buf_size            -= tmp_size;
    }

    return read_size;
}

/* ****************************** end of file ****************************** */
