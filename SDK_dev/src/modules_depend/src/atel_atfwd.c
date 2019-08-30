/******************************************************************************
*@file    example_atfwd.c
*@brief   example of AT forward service
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__ATEL_BG96_APP__)
/*===========================================================================
						   Header file
===========================================================================*/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "qapi_fs_types.h"
#include "qapi_status.h"
#include "qapi_atfwd.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "quectel_gpio.h"
#include "atel_atfwd.h"
#include "atel_gpio.h"

/**************************************************************************
*								  GLOBAL
***************************************************************************/

/* begin: queue for communicate */
static TX_QUEUE tx_queue_handle;
static TASK_MSG task_comm[QT_Q_MAX_INFO_NUM];
/* end */

TX_BYTE_POOL *byte_pool_uart;
TX_BYTE_POOL *byte_pool_at;

#define UART_BYTE_POOL_SIZE		10*8*1024
UCHAR free_memory_uart[UART_BYTE_POOL_SIZE];
UCHAR free_memory_at[UART_BYTE_POOL_SIZE];


/* uart rx tx buffer */
static char *rx_buff = NULL;
static char *tx_buff = NULL;

/* uart config para*/
static QT_UART_CONF_PARA uart_conf =
{
	NULL,
	QT_UART_PORT_02,
	NULL,
	0,
	NULL,
	0,
	115200
};

static int qexample_val = 0;
char at_cmd_rsp[2048] = {0};

TX_BYTE_POOL * byte_pool_qcli;

/**************************************************************************
*                                 FUNCTION
***************************************************************************/
static int qt_atoi(char *str)
{
    int res = 0, i;

    for (i = 0; str[i] != '\0' && str[i] != '.'; ++i)
    {
        res = res*10 + (str[i] - '0');
    }

    return res;
}

static int qt_termintocomma(char * s1, char * s2, size_t n)
{
  unsigned char c1, c2;
  size_t i=0;

  if (strlen(s1) > 0)
  {
    for(i=0; i<n; i++)
    {
      c1 = (unsigned char)(*(s1+i));
      c2 = (unsigned char)(*(s1+i+1));
      if ((c1 !='\0'))
      {
        s2[i] = c1;
      }
      else if ((c1 =='\0') && (c2 !='\0'))
      {
        s2[i] = ',';
      }
      else
      {
        s2[i] = '\0';
        return i;
      }
    } 
    return i;
  }
  return 0;
}

static uint8 ToUper( uint8 ch )
{
   if( ch >='a' && ch <= 'z' )
      return (uint8)( ch + ('A' - 'a') );
   return ch;
}

static uint8 stricmp(const char* s1, const char* s2)
{
	uint16 nLen1,nLen2;
	uint16 i=0;
	if(!s1||!s2)
		return 2;
	nLen1=strlen((char*)s1);
	nLen2=strlen((char*)s2);

	if(nLen1>nLen2)
		return 1;
	else if(nLen1<nLen2)
		return 1;
	while(i<nLen1)
	{
	if(ToUper((uint8)s1[i])!=ToUper((uint8)s2[i]))
	return 1;	
	i++;
	}
  return 0;
}

static int strncasecmp(const char * s1, const char * s2, size_t n)
{
  unsigned char c1, c2;
  int diff;

  if (n > 0)
  {
    do
    {
      c1 = (unsigned char)(*s1++);
      c2 = (unsigned char)(*s2++);
      if (('A' <= c1) && ('Z' >= c1))
      {
        c1 = c1 - 'A' + 'a';
      }
      if (('A' <= c2) && ('Z' >= c2))
      {
        c2 = c2 - 'A' + 'a';
      }
      diff = c1 - c2;

      if (0 != diff)
      {
        return diff;
      }

      if ('\0' == c1)
      {
        break;
      }
    } while (--n);
  }
  return 0;
}

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  uart_uart_rx_cb
@brief
  uart rx callback handler.
*/
static void uart_rx_cb_at(uint32_t num_bytes, void *cb_data)
{
	QT_UART_CONF_PARA *uart_conf = (QT_UART_CONF_PARA*)cb_data;
	TASK_MSG rxcb;

	qapi_Status_t status;
	
	atel_dbg_print("[get raw data from BLE tx] uart_conf->rx_buff %d@len,%s@string",uart_conf->rx_len,uart_conf->rx_buff);
		
	if(num_bytes == 0)
	{
		uart_recv(uart_conf);
		return;
	}
	else if(num_bytes >= uart_conf->rx_len)
	{
		num_bytes = uart_conf->rx_len;
	}
	
	uart_recv(uart_conf);
	
	/* uart2_conf->rx_buff store the tx data from BLE */
	memcpy(&at_cmd_rsp[strlen(at_cmd_rsp)],uart_conf->rx_buff,num_bytes);
	
	atel_dbg_print("[get respond from BLE tx] uart_conf->rx_buff %d@len,%s@string",uart_conf->rx_len,uart_conf->rx_buff);

	status = tx_queue_send(&tx_queue_handle, &rxcb,TX_WAIT_FOREVER );
	if (TX_SUCCESS != status)
	{
		//qt_uart_dbg(uart1_conf.hdlr, "[task_create 1 ] tx_queue_send failed with status %d", status);
	}
}


void uart_init_at(QT_UART_CONF_PARA *uart_conf)
{
	//qapi_Status_t status;
	qapi_UART_Open_Config_t uart_cfg;
	QAPI_Flow_Control_Type uart_fc_type = QAPI_FCTL_OFF_E;

	uart_cfg.baud_Rate			= uart_conf->baudrate;
	uart_cfg.enable_Flow_Ctrl	= QAPI_FCTL_OFF_E;
	uart_cfg.bits_Per_Char		= QAPI_UART_8_BITS_PER_CHAR_E;
	uart_cfg.enable_Loopback 	= 0;
	uart_cfg.num_Stop_Bits		= QAPI_UART_1_0_STOP_BITS_E;
	uart_cfg.parity_Mode 		= QAPI_UART_NO_PARITY_E;
	uart_cfg.rx_CB_ISR			= (qapi_UART_Callback_Fn_t)&uart_rx_cb_at;
	uart_cfg.tx_CB_ISR			= NULL;

	qapi_UART_Open(&uart_conf->hdlr, uart_conf->port_id, &uart_cfg);

	qapi_UART_Power_On(uart_conf->hdlr);

	qapi_UART_Ioctl(uart_conf->hdlr, QAPI_SET_FLOW_CTRL_E, &uart_fc_type);
}

void uart_deinit_at(QT_UART_CONF_PARA *uart_conf)
{
	qapi_UART_Close(uart_conf->hdlr);

	qapi_UART_Power_Off(uart_conf->hdlr);

	return; 
}


/*
@func
  uart_recv
@brief
  Start a uart receive action.
*/
void uart_recv_at(QT_UART_CONF_PARA *uart_conf)
{
	//qapi_Status_t status;
	qapi_UART_Receive(uart_conf->hdlr, uart_conf->rx_buff, uart_conf->rx_len, (void*)uart_conf);
	//IOT_DEBUG("QT# qapi_UART_Receive [%d] status %d", (qapi_UART_Port_Id_e)uart_conf->port_id, status);
}


boolean cmd_ctrl_gpio(char *tmp_params)
{
	int index = 0;
	for ( ; index < ATCMDS_SIZE; ++index )
	{
		if(stricmp(tmp_params, atcmd_gpio_table[index].cmdstr) == 0)
		{

			gpio_value_control(atcmd_gpio_table[index].gpio_phy_port, atcmd_gpio_table[index].on_enable_flag);
			return TRUE;
		}
		else if(stricmp(tmp_params, atcmd_gpio_table[index].oppcmdstr) == 0)
		{
			gpio_value_control(atcmd_gpio_table[index].gpio_phy_port, !atcmd_gpio_table[index].on_enable_flag);
			//realse gpio resorces
			//qapi_TLMM_Release_Gpio_ID(&tlmm_config[atcmd_gpio_table[index].gpio_phy_port], gpio_id_tbl[atcmd_gpio_table[index].gpio_phy_port]);
			return TRUE;
		}
	}

	return FALSE;
	
}

void atfwd_cmd_handler_cb(boolean is_reg, char *atcmd_name,
                                 uint8* at_fwd_params, uint8 mask,
                                 uint32 at_handle)
{
    char buff[32] = {0};
    int  tmp_val = 0;
    qapi_Status_t ret = QAPI_ERROR;
    
    atel_dbg_print("atfwd_cmd_handler_cb is called, atcmd_name:[%s] mask:[%d]\n", atcmd_name, mask);
	atel_dbg_print("atfwd_cmd_handler_cb is called,  is_reg:[%d]\n", is_reg);

	if(is_reg)  //Registration Successful,is_reg return 1 
	{
		if(mask==QUEC_AT_MASK_EMPTY_V01)
		{
			qt_uart_dbg(uart_conf.hdlr,"Atcmd %s is registered\n",atcmd_name);
			return;

		}
    	if( !strncasecmp(atcmd_name, "+QEXAMPLE",strlen(atcmd_name)) )
	    {
	        //Execute Mode
	        if ((QUEC_AT_MASK_NA_V01) == mask)//AT+QEXAMPLE
	        {
	            ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);
	        }
	        //Read Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+QEXAMPLE?
	        {
	            snprintf(buff, sizeof(buff), "+QEXAMPLE: %d", qexample_val);
	            ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
	        }
	        //Test Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+QEXAMPLE=?
	        {
	            snprintf(buff, sizeof(buff), "+QEXAMPLE: (0-2)");
	            ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
	        }
	        //Write Mode
	        else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_AR_V01) == mask)//AT+QEXAMPLE=<value>
	        {
	            tmp_val = qt_atoi((char*)at_fwd_params);
	            if(tmp_val >= 0 && tmp_val <= 2)
	            {
	                qexample_val = tmp_val;
	                ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);
	            }
	            else
	            {
	                ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_ERROR_V01);
	            }  
	        }
	    }
	    else
	    {
	        ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_ERROR_V01);
	        //ret = qapi_atfwd_send_urc_resp(atcmd_name, "this is asiatelco");
	    }

    	qt_uart_dbg(uart_conf.hdlr,"[%s] send resp, ret = %d\n", atcmd_name, ret);
	}
}

/*
@func
	quectel_task_entry
@brief
	Entry function for task. 
*/
int atel_mdm_ble_entry(void)
{
	qapi_Status_t retval = QAPI_ERROR;
	int ret = -1;

	/* wait 5sec for device startup */
	qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	txm_module_object_allocate(&byte_pool_at, sizeof(TX_BYTE_POOL));
	tx_byte_pool_create(byte_pool_at, "byte pool 0", free_memory_at, 10*1024);

	ret = txm_module_object_allocate(&byte_pool_uart, sizeof(TX_BYTE_POOL));
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("txm_module_object_allocate [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_pool_create(byte_pool_uart, "Sensor application pool", free_memory_uart, UART_BYTE_POOL_SIZE);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_pool_create [byte_pool_sensor] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&rx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [rx_buff] failed, %d", ret);
		return ret;
	}

	ret = tx_byte_allocate(byte_pool_uart, (VOID *)&tx_buff, 4*1024, TX_NO_WAIT);
	if(ret != TX_SUCCESS)
	{
		IOT_DEBUG("tx_byte_allocate [tx_buff] failed, %d", ret);
		return ret;
	}

	uart_conf.tx_buff = tx_buff;
	uart_conf.rx_buff = rx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;
	/* uart init */
	uart_init(&uart_conf);
	/* start uart receive */
	uart_recv(&uart_conf);
	/* prompt task running */
    
	qt_uart_dbg(uart_conf.hdlr,"ATFWD Example entry...\n");

	if (qapi_atfwd_Pass_Pool_Ptr(atfwd_cmd_handler_cb, byte_pool_at) != QAPI_OK)
	{
		qt_uart_dbg(uart_conf.hdlr, "Unable to alloc User space memory fail state  %x" ,0);
	  										
	}
	
    retval = qapi_atfwd_reg("+QEXAMPLE", atfwd_cmd_handler_cb);
    if(retval != QAPI_OK)
    {
        qt_uart_dbg(uart_conf.hdlr,"qapi_atfwd_reg  fail\n");
    }
    else
    {
        qt_uart_dbg(uart_conf.hdlr,"qapi_atfwd_reg ok!\n");
    }
    while(1)
    {
	    /* wait 5sec */
	    qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);
    }

    return 0;
}

#endif/*end of __ATEL_BG96_APP__*/


