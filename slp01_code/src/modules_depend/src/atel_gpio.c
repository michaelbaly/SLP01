/******************************************************************************
*@file    example_gpio.c
*@brief   example for gpio operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__ATEL_BG96_APP__)
#include "qapi_tlmm.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "qapi_uart.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "atel_gpio.h"
#include "quectel_gpio.h"


/**************************************************************************
*                                 GLOBAL
***************************************************************************/
/*  !!! This Pin Enumeration Only Applicable BG96-OPEN Project !!!
 */
GPIO_MAP_TBL gpio_map_tbl[PIN_E_GPIO_MAX] = {
/* PIN NUM,     PIN NAME,    GPIO ID  GPIO FUNC */
	{  4, 		"GPIO01",  		23, 	 0},
	{  5, 		"GPIO02",  		20, 	 0},
	{  6, 		"GPIO03",  		21, 	 0},
	{  7, 		"GPIO04",  		22, 	 0},
	{ 18, 		"GPIO05",  		11, 	 0},
	{ 19, 		"GPIO06",  		10, 	 0},
	{ 22, 		"GPIO07",  		 9, 	 0},
	{ 23, 		"GPIO08",  	 	 8, 	 0},
	{ 26, 		"GPIO09",  		15, 	 0},
	{ 27, 		"GPIO10",  		12, 	 0},
	{ 28, 		"GPIO11",  		13, 	 0},
	{ 40, 		"GPIO19",  		19, 	 0},
	{ 41, 		"GPIO20",  		18, 	 0},
	{ 64, 		"GPIO21",  		07, 	 0},
};

/* gpio id table */
qapi_GPIO_ID_t gpio_id_tbl[PIN_E_GPIO_MAX];

/* gpio tlmm config table */
qapi_TLMM_Config_t tlmm_config[PIN_E_GPIO_MAX];
	
/* Modify this pin num which you want to test */
//MODULE_PIN_ENUM  g_gpio_pin_num = PIN_E_GPIO_11;

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  gpio_config
@brief
  [in]  m_pin
  		MODULE_PIN_ENUM type; the GPIO pin which customer want used for operation;
  [in]  gpio_dir
  		qapi_GPIO_Direction_t type; GPIO pin direction.
  [in]  gpio_pull
  		qapi_GPIO_Pull_t type; GPIO pin pull type.
  [in]  gpio_drive
  		qapi_GPIO_Drive_t type; GPIO pin drive strength. 
*/
void gpio_config(MODULE_PIN_ENUM m_pin,
				 qapi_GPIO_Direction_t gpio_dir,
				 qapi_GPIO_Pull_t gpio_pull,
				 qapi_GPIO_Drive_t gpio_drive
				 )
{
	qapi_Status_t status = QAPI_OK;

	tlmm_config[m_pin].pin   = gpio_map_tbl[m_pin].gpio_id;
	tlmm_config[m_pin].func  = gpio_map_tbl[m_pin].gpio_func;
	tlmm_config[m_pin].dir   = gpio_dir;
	tlmm_config[m_pin].pull  = gpio_pull;
	tlmm_config[m_pin].drive = gpio_drive;

	// the default here
	status = qapi_TLMM_Get_Gpio_ID(&tlmm_config[m_pin], &gpio_id_tbl[m_pin]);
	qt_uart_dbg("ATEL# gpio_id[%d] status = %d", gpio_map_tbl[m_pin].gpio_id, status);
	if (status == QAPI_OK)
	{
		status = qapi_TLMM_Config_Gpio(gpio_id_tbl[m_pin], &tlmm_config[m_pin]);
		qt_uart_dbg("ATEL# gpio_id[%d] status = %d", gpio_map_tbl[m_pin].gpio_id, status);
		if (status != QAPI_OK)
		{
			qt_uart_dbg("ATEL# gpio_config failed");
		}
	}
}

/*
@func
  atel_led_on
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int mm16_lan_power_on(MODULE_PIN_ENUM m_pin)
{
    //qt_uart_dbg("ATEL# ### Test pin is %d, gpio_id is %d ###", m_pin, gpio_map_tbl[m_pin].gpio_id);
	gpio_config(m_pin, QAPI_GPIO_OUTPUT_E, QAPI_GPIO_NO_PULL_E, QAPI_GPIO_2MA_E);	
    qapi_TLMM_Drive_Gpio(gpio_id_tbl[m_pin], gpio_map_tbl[m_pin].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
	//qt_uart_dbg("ATEL# Set %d QAPI_GPIO_HIGH_VALUE_E status = %d", m_pin, status);
	//qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
	
	return 0;
}

/*
@func
  atel_led_on
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int atel_led_on(MODULE_PIN_ENUM m_pin)
{
	qapi_Status_t status;

    qt_uart_dbg("ATEL# ### Test pin is %d, gpio_id is %d ###", m_pin, gpio_map_tbl[m_pin].gpio_id);
	gpio_config(m_pin, QAPI_GPIO_OUTPUT_E, QAPI_GPIO_PULL_UP_E, QAPI_GPIO_16MA_E);	
	status = qapi_TLMM_Drive_Gpio(gpio_id_tbl[m_pin], gpio_map_tbl[m_pin].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
	qt_uart_dbg("ATEL# Set %d QAPI_GPIO_HIGH_VALUE_E status = %d", m_pin, status);
	//qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);

	return 0;
}

/*
@func
  atel_led_off
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int atel_led_off(MODULE_PIN_ENUM m_pin)
{
	qapi_Status_t status;
	
	status = qapi_TLMM_Drive_Gpio(gpio_id_tbl[m_pin], gpio_map_tbl[m_pin].gpio_id, QAPI_GPIO_LOW_VALUE_E);
	qt_uart_dbg("ATEL# Set %d QAPI_GPIO_LOW_VALUE_E status = %d", m_pin, status);
	//qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);

	status = qapi_TLMM_Release_Gpio_ID(&tlmm_config[m_pin], gpio_id_tbl[m_pin]);
	qt_uart_dbg("ATEL# release %d status = %d", m_pin, status);
	
	return 0;
}


#endif /*__ATEL_BG96_APP__*/

