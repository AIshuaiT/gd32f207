/**
 ***************************** Learn software ******************************
 *
 * This file is part of LN firmware.
 * File name : thread_lora_api.c
 * Arthor    : Test
 * Date      : Apr 7th, 2017
 *
 ******************************************************************************
 */
 
/**
 * CHANGE LOGS
 ******************************************************************************
 * DATE            BY           DESCRIPTION
 * 2017-04-07      Test          First version.
 ******************************************************************************
 */
 

/**
 ******************************************************************************
 *                                  INCLUDES
 ******************************************************************************
 */

#include "loragw_hal.h"
#include "thread_lora.h" 
#include "external_flash.h"

/**
 ******************************************************************************
 *                                   MACROS
 ******************************************************************************
 */ 

#define DEFAULT_RSSI_OFFSET  (-176.0f)
#define DEFAULT_NOTCH_FREQ   129000U
 
/**
 ******************************************************************************
 *                               TYPE DEFINITION
 ******************************************************************************
 */

static rt_uint32_t base_freq = 471100000;
static rt_uint8_t  base_powr = 17;

/**
 ******************************************************************************
 *                              GLOBAL VARIABLES
 ******************************************************************************
 */

struct rt_mutex     mutex_lora;
 
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

static void parse_module_config(void);
 
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
 * @brief configure rf_front and 8 receive channels.
 */
static void parse_module_config(void)
{
	struct lgw_conf_board_s boardconf;
    struct lgw_conf_rxrf_s rfconf;
    struct lgw_conf_rxif_s ifconf;

	enum lgw_radio_type_e radio_type = LGW_RADIO_TYPE_SX1255;
	rt_uint8_t clocksource = 1; /* Radio B is source by default */

    base_freq = g_wnc_config.lora_config.rf_freq;
    base_powr = g_wnc_config.lora_config.rf_power;
    
	/* set configuration for board */
    rt_memset(&boardconf, 0, sizeof(boardconf));

    boardconf.lorawan_public = true;
    boardconf.clksrc = clocksource;
    lgw_board_setconf(boardconf);

	/* set configuration for RF chains */
    rt_memset(&rfconf, 0, sizeof(rfconf));

    rfconf.enable = true;
    rfconf.freq_hz = base_freq + 1400000;
    rfconf.rssi_offset = DEFAULT_RSSI_OFFSET;
    rfconf.type = radio_type;
    rfconf.tx_enable = true;
    rfconf.tx_notch_freq = DEFAULT_NOTCH_FREQ;
    lgw_rxrf_setconf(0, rfconf); /* radio A, f0 */

    rfconf.enable = true;
    rfconf.freq_hz = base_freq + 400000;
    rfconf.rssi_offset = DEFAULT_RSSI_OFFSET;
    rfconf.type = radio_type;
    rfconf.tx_enable = false;
    lgw_rxrf_setconf(1, rfconf); /* radio B, f1 */

	/* set configuration for LoRa multi-SF channels (bandwidth cannot be set) */
    rt_memset(&ifconf, 0, sizeof(ifconf));

    ifconf.enable = true;
    ifconf.rf_chain = 1;
    ifconf.freq_hz = -400000;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(0, ifconf); /* chain 0: LoRa 125kHz, all SF, on f1 - 0.4 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 1;
    ifconf.freq_hz = -200000;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(1, ifconf); /* chain 1: LoRa 125kHz, all SF, on f1 - 0.2 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 1;
    ifconf.freq_hz = 0;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(2, ifconf); /* chain 2: LoRa 125kHz, all SF, on f1 - 0.0 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 1;
    ifconf.freq_hz = 200000;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(3, ifconf); /* chain 3: LoRa 125kHz, all SF, on f0 - 0.4 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 1;
    ifconf.freq_hz = 400000;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(4, ifconf); /* chain 4: LoRa 125kHz, all SF, on f0 - 0.2 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 0;
    ifconf.freq_hz = -400000;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(5, ifconf); /* chain 5: LoRa 125kHz, all SF, on f0 + 0.0 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 0;
    ifconf.freq_hz = -200000;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(6, ifconf); /* chain 6: LoRa 125kHz, all SF, on f0 + 0.2 MHz */

    ifconf.enable = true;
    ifconf.rf_chain = 0;
    ifconf.freq_hz = 0;
    ifconf.datarate = DR_LORA_MULTI;
    lgw_rxif_setconf(7, ifconf); /* chain 7: LoRa 125kHz, all SF, on f0 + 0.4 MHz */

    /* set configuration for LoRa 'stand alone' channel */
    rt_memset(&ifconf, 0, sizeof(ifconf));
    ifconf.enable = true;
    ifconf.rf_chain = 0;
    ifconf.freq_hz = 200000;
    ifconf.bandwidth = BW_250KHZ;
    ifconf.datarate = DR_LORA_SF10;
    lgw_rxif_setconf(8, ifconf); /* chain 8: LoRa 250kHz, SF10, on f0 MHz */

    /* set configuration for FSK channel */
    rt_memset(&ifconf, 0, sizeof(ifconf));
    ifconf.enable = true;
    ifconf.rf_chain = 1;
    ifconf.freq_hz = 0;
    ifconf.bandwidth = BW_250KHZ;
    ifconf.datarate = 64000;
    lgw_rxif_setconf(9, ifconf); /* chain 9: FSK 64kbps, on f1 MHz */    
}

/**
 * @brief  start sx1301 module 
 * @retval 0 for success, others for failure
 */
int start_lora_module(void)
{
    /* set RF params */
    parse_module_config();
    
	/* start sx1301 module */
	return lgw_start();
}

/**
 * @brief  transform datarate from metadata to number
 * @param  dr: datarate from lora rx package
 * @retval N of lora datarate SF(N)
 */
rt_uint8_t lora_get_datarate(rt_uint8_t dr)
{
    switch(dr)
    {
        case DR_LORA_SF7:
            return 7;
        case DR_LORA_SF8:
            return 8;
        case DR_LORA_SF9:
            return 9;
        case DR_LORA_SF10:
            return 10;
        default:
            return 0;
    }
}

/**
 * @brief  transform datarate from number to metadata
 * @param  dr: datarate from lora tx package
 * @retval datarate value sx1301 needed
 */
rt_uint8_t lora_set_datarate(rt_uint8_t dr)
{
    switch(dr)
    {
        case 7:
            return DR_LORA_SF7;
        case 8:
            return DR_LORA_SF8;
        case 9:
            return DR_LORA_SF9;
        case 10:
            return DR_LORA_SF10;
        default:
            return 0;
    }
}

/**
 * @brief  get lora tx frequency
 * @retval tx frequency in hz
 */
rt_uint32_t get_tx_freq(void)
{
    return (base_freq + 1800000);
}

/**
 * @brief  get lora tx power
 * @retval tx power in dBm
 */
rt_int8_t get_tx_power(void)
{
    return (base_powr);
}
 
/* ****************************** end of file ****************************** */
