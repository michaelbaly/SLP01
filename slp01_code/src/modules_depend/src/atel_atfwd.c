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
#if defined(__EXAMPLE_ATFWD__)
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
#include "atel_atfwd.h"

/**************************************************************************
*								  GLOBAL
***************************************************************************/

/* uart rx tx buffer */
static char rx1_buff[1024];
static char tx1_buff[1024];

/* uart config para*/
static QT_UART_CONF_PARA uart1_conf =
{
	NULL,
	QT_UART_PORT_01,
	tx1_buff,
	sizeof(tx1_buff),
	rx1_buff,
	sizeof(rx1_buff),
	115200
};

/* uart2 rx tx buffer */
static char rx2_buff[2048];
static char tx2_buff[2048];

/* uart config para*/
static QT_UART_CONF_PARA uart2_conf =
{
	NULL,
	QT_UART_PORT_02,
	tx2_buff,
	sizeof(tx2_buff),
	rx2_buff,
	sizeof(rx2_buff),
	115200
};

static TX_QUEUE tx_queue_handle;
static TASK_MSG task_comm[QT_Q_MAX_INFO_NUM];

static int qexample_val = 0;
char at_cmd_rsp[2048] = {0};
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
	
/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  qt_uart_dbg
@brief
  Output the debug log. 
*/
void atel_dbg_print(const char* fmt, ...)
{
	char log_buf[256] = {0};

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
	va_end( ap );

	qapi_atfwd_send_urc_resp("ATEL", log_buf);
	qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_MSEC, true);
}

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
	IOT_DEBUG("QT# gpio_id[%d] status = %d", gpio_map_tbl[m_pin].gpio_id, status);
	if (status == QAPI_OK)
	{
		status = qapi_TLMM_Config_Gpio(gpio_id_tbl[m_pin], &tlmm_config[m_pin]);
		IOT_DEBUG("QT# gpio_id[%d] status = %d", gpio_map_tbl[m_pin].gpio_id, status);
		if (status != QAPI_OK)
		{
			IOT_DEBUG("QT# gpio_config failed");
		}
	}
}

void gpio_dir_control(MODULE_PIN_ENUM pin_num, qapi_GPIO_Direction_t direction)
{
	gpio_config(pin_num, direction, QAPI_GPIO_NO_PULL_E, QAPI_GPIO_2MA_E);	
}

void gpio_value_control(MODULE_PIN_ENUM pin_num, boolean vaule)
{

	if(vaule)
		qapi_TLMM_Drive_Gpio(gpio_id_tbl[pin_num], gpio_map_tbl[pin_num].gpio_id, QAPI_GPIO_HIGH_VALUE_E);
	else
		qapi_TLMM_Drive_Gpio(gpio_id_tbl[pin_num], gpio_map_tbl[pin_num].gpio_id, QAPI_GPIO_LOW_VALUE_E);
}
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
			break;
		}
		else if(stricmp(tmp_params, atcmd_gpio_table[index].oppcmdstr) == 0)
		{
			gpio_value_control(atcmd_gpio_table[index].gpio_phy_port, ~atcmd_gpio_table[index].on_enable_flag);
			//realse gpio resorces
			qapi_TLMM_Release_Gpio_ID(&tlmm_config[atcmd_gpio_table[index].gpio_phy_port], gpio_id_tbl[atcmd_gpio_table[index].gpio_phy_port]);
			break;
		}
	}

	return FALSE;
	
}

void atfwd_cmd_handler_cb(boolean is_reg, char *atcmd_name,
                                 uint8* at_fwd_params, uint8 mask,
                                 uint32 at_handle)
{
    char buff[32] = {0};
    qapi_Status_t ret = QAPI_ERROR;
    
    //atel_dbg_print("atfwd_cmd_handler_cb is called, is_reg:[%d]atcmd_name:[%s] mask:[%d]\n", is_reg,atcmd_name, mask);
    if (is_reg)   //Registration Successful,is_reg return 1 
    {
        //atel_dbg_print("Atcmd %s is registered\n",atcmd_name);
        if(mask ==0)
        {
            return;
        }
        if( !strncasecmp(atcmd_name, "+BLE",strlen(atcmd_name)) )
        {
            //Execute Mode
            if ((QUEC_AT_MASK_NA_V01) == mask)//AT+BLE
            {
                ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);
            }
            //Read Mode
            else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+BLE?
            {
                snprintf(buff, sizeof(buff), "+BLE: %d", qexample_val);
                ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
            }
            //Test Mode
            else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_QU_V01) == mask)//AT+BLE=?
            {
                snprintf(buff, sizeof(buff), "+BLE: (0-2)");
                ret = qapi_atfwd_send_resp(atcmd_name, buff, QUEC_AT_RESULT_OK_V01);
            }
            //Write Mode
            else if ((QUEC_AT_MASK_NA_V01 | QUEC_AT_MASK_EQ_V01 | QUEC_AT_MASK_AR_V01) == mask)//AT+BLE=<value>  AT+MCU=<"str">
            {
                char tmp_params[128] = {0};

				
				//atel_dbg_print("content of at_fwd_params:%s", at_fwd_params);
				
                ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_OK_V01);//send atcmd  Reponse and then atcmd to uart2
                qt_termintocomma((char*)at_fwd_params,tmp_params,sizeof(tmp_params));
                if(memcmp(tmp_params,"##",2) == 0)
                {
                	//send command to uart2
                	qt_uart_dbg(uart2_conf.hdlr, "%s", tmp_params);
					atel_dbg_print("[send to BLE rx] tmp_params %s@string", tmp_params);
                }
				
                if (!cmd_ctrl_gpio(tmp_params))
                {
					atel_dbg_print("command does't support: %s", tmp_params);
                }
				else
                {
					atel_dbg_print("content of atcmd_name:%s, at_fwd_params:%s", atcmd_name, at_fwd_params);
                	qapi_atfwd_send_urc_resp(atcmd_name, at_fwd_params);
                }
				
            }
        }
        else
        {
            ret = qapi_atfwd_send_resp(atcmd_name, "", QUEC_AT_RESULT_ERROR_V01);
        }
        qt_uart_dbg(uart1_conf.hdlr,"[%s] send resp, ret = %d\n", atcmd_name, ret);
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
	TASK_MSG rxdata;
	char buff[2048] = {0};
	char atcmd_name[32] = {0};
        
	/* uart 2 init */
	uart_init_at(&uart2_conf);
	/* start uart 2 receive */
	uart_recv_at(&uart2_conf);
	
    retval = qapi_atfwd_reg("+BLE", atfwd_cmd_handler_cb);
    if(retval != QAPI_OK)
    {
        //qt_uart_dbg(uart1_conf.hdlr,"ATFWD register [%s] faild! ret = %d\n", "+MCU", retval);
    }
    else
    {
        //qt_uart_dbg(uart1_conf.hdlr,"ATFWD register [%s] complete!\n", "+MCU");
    }
    
    /* create a new queue : q_task_comm */
    retval = tx_queue_create(&tx_queue_handle,
							 "q_task_comm",
							 TX_16_ULONG,
							 task_comm,
							 QT_Q_MAX_INFO_NUM *sizeof(TASK_MSG)
							 );
	
    if (TX_SUCCESS != retval)
    {
    	//qt_uart_dbg(uart1_conf.hdlr, "tx_queue_create failed with status %d", retval);
    }
	

    while(1)
    {
	
	    retval = tx_queue_receive(&tx_queue_handle, &rxdata, TX_WAIT_FOREVER);

	    if(TX_SUCCESS != retval)
	    {
	    	//qt_uart_dbg(uart1_conf.hdlr, "[Main task_create] tx_queue_receive failed with status %d", retval);
	    }
		
	    /* wait 10 mill sec */
	    qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
		
	    //Active Report
	    atel_dbg_print("[get respond] at_cmd_rsp %d@len,%s@string",strlen(at_cmd_rsp),at_cmd_rsp);
	    if(strlen(at_cmd_rsp) > 0)
	    {
		    memset(atcmd_name, 0x00,sizeof(atcmd_name));
		    memcpy(atcmd_name, "+BLE",4);
		    snprintf(buff, sizeof(buff), "%s", at_cmd_rsp);
		    retval = qapi_atfwd_send_urc_resp(atcmd_name, buff);
		    memset(at_cmd_rsp, 0, sizeof(at_cmd_rsp));
		    //memset(buff, 0, sizeof(buff));
	    }
    }

    return 0;
}

#endif/*end of __EXAMPLE_ATFWD__*/


