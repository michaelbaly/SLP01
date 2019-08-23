/******************************************************************************
*@file    example_tcpclient.c
*@brief   Detection of network state and notify the main task to create tcp client.
*         If server send "Exit" to client, client will exit immediately.
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
#include "qapi_socket.h"
#include "qapi_dss.h"
#include "qapi_netservices.h"

#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "atel_udpclient.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define QL_DEF_APN	        "CMNET"
#define DSS_ADDR_INFO_SIZE	5
#define DSS_ADDR_SIZE       16

#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))

#define QUEC_UDP_UART_DBG
#ifdef QUEC_UDP_UART_DBG
#define UDP_UART_DBG(...)	\
{\
	udp_uart_debug_print(__VA_ARGS__);	\
}
#else
#define UDP_UART_DBG(...)
#endif

#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_PRIORITY      (180)
#define BYTE_POOL_SIZE       (1024 * 16)

#define CMD_BUF_SIZE  100

#define DEFAULT_PUB_TIME 5

/*===========================================================================
                           Global variable
===========================================================================*/
/* UDPClient dss thread handle */
#ifdef QAPI_TXM_MODULE
	static TX_THREAD *dss_thread_handle; 
#else
	static TX_THREAD _dss_thread_handle;
	static TX_THREAD *ts_thread_handle = &_dss_thread_handle;
#endif

static unsigned char udp_dss_stack[THREAD_STACK_SIZE];

TX_EVENT_FLAGS_GROUP *udp_signal_handle;

qapi_DSS_Hndl_t udp_dss_handle = NULL;	            /* Related to DSS netctrl */

static char apn[QUEC_APN_LEN];					/* APN */
static char apn_username[QUEC_APN_USER_LEN];	/* APN username */
static char apn_passwd[QUEC_APN_PASS_LEN];		/* APN password */

/* @Note: If netctrl library fail to initialize, set this value will be 1,
 * We should not release library when it is 1. 
 */
signed char udp_netctl_lib_status = DSS_LIB_STAT_INVALID_E;
unsigned char udp_datacall_status = DSS_EVT_INVALID_E;

TX_BYTE_POOL *byte_pool_udp;
//#define UDP_BYTE_POOL_SIZE		10*8*1024
#define UDP_BYTE_POOL_SIZE		8*1024

UCHAR free_memory_udp[UDP_BYTE_POOL_SIZE];

/* uart rx tx buffer */
static char *uart_rx_buff = NULL;	/*!!! should keep this buffer as 4K Bytes */
static char *uart_tx_buff = NULL;

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

/*===========================================================================
                               FUNCTION
===========================================================================*/
void udp_uart_dbg_init()
{
  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_udp, (VOID *)&uart_rx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_rx_buff] failed!");
    	return;
  	}

  	if (TX_SUCCESS != tx_byte_allocate(byte_pool_udp, (VOID *)&uart_tx_buff, 4*1024, TX_NO_WAIT))
  	{
  		IOT_DEBUG("tx_byte_allocate [uart_tx_buff] failed!");
    	return;
  	}

    uart_conf.rx_buff = uart_rx_buff;
	uart_conf.tx_buff = uart_tx_buff;
	uart_conf.tx_len = 4096;
	uart_conf.rx_len = 4096;

	/* debug uart init 			*/
	uart_init(&uart_conf);
	/* start uart receive */
	uart_recv(&uart_conf);
}
void udp_uart_debug_print(const char* fmt, ...) 
{
	va_list arg_list;
    char dbg_buffer[128] = {0};
    
	va_start(arg_list, fmt);
    vsnprintf((char *)(dbg_buffer), sizeof(dbg_buffer), (char *)fmt, arg_list);
    va_end(arg_list);
		
    qapi_UART_Transmit(uart_conf.hdlr, dbg_buffer, strlen(dbg_buffer), NULL);
    qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);   //50
}


/*
@func
	dss_net_event_cb
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static void udp_net_event_cb
( 
	qapi_DSS_Hndl_t 		hndl,
	void 				   *user_data,
	qapi_DSS_Net_Evt_t 		evt,
	qapi_DSS_Evt_Payload_t *payload_ptr 
)
{
	qapi_Status_t status = QAPI_OK;
	
	atel_dbg_print("Data test event callback, event: %d\n", evt);

	switch (evt)
	{
		case QAPI_DSS_EVT_NET_IS_CONN_E:
		{
			atel_dbg_print("Data Call Connected.\n");
			//udp_show_sysinfo();
			/* Signal main task */
  			tx_event_flags_set(udp_signal_handle, DSS_SIG_EVT_CONN_E, TX_OR);
			udp_datacall_status = DSS_EVT_NET_IS_CONN_E;
			
			break;
		}
		case QAPI_DSS_EVT_NET_NO_NET_E:
		{
			atel_dbg_print("Data Call Disconnected.\n");
			
			if (DSS_EVT_NET_IS_CONN_E == udp_datacall_status)
			{
				/* Release Data service handle and netctrl library */
				if (udp_dss_handle)
				{
					status = qapi_DSS_Rel_Data_Srvc_Hndl(udp_dss_handle);
					if (QAPI_OK == status)
					{
						atel_dbg_print("Release data service handle success\n");
						tx_event_flags_set(udp_signal_handle, DSS_SIG_EVT_EXIT_E, TX_OR);
					}
				}
				
				if (DSS_LIB_STAT_SUCCESS_E == udp_netctl_lib_status)
				{
					qapi_DSS_Release(QAPI_DSS_MODE_GENERAL);
				}
			}
			else
			{
				/* DSS status maybe QAPI_DSS_EVT_NET_NO_NET_E before data call establishment */
				tx_event_flags_set(udp_signal_handle, DSS_SIG_EVT_NO_CONN_E, TX_OR);
			}

			break;
		}
		default:
		{
			atel_dbg_print("Data Call status is invalid.\n");
			
			/* Signal main task */
  			tx_event_flags_set(udp_signal_handle, DSS_SIG_EVT_INV_E, TX_OR);
			udp_datacall_status = DSS_EVT_INVALID_E;
			break;
		}
	}
}

void udp_show_sysinfo(void)
{
	int i   = 0;
	int j 	= 0;
	unsigned int len = 0;
	uint8 buff[DSS_ADDR_SIZE] = {0}; 
	qapi_Status_t status;
	qapi_DSS_Addr_Info_t info_ptr[DSS_ADDR_INFO_SIZE];

	status = qapi_DSS_Get_IP_Addr_Count(udp_dss_handle, &len);
	if (QAPI_ERROR == status)
	{
		atel_dbg_print("Get IP address count error\n");
		return;
	}
		
	status = qapi_DSS_Get_IP_Addr(udp_dss_handle, info_ptr, len);
	if (QAPI_ERROR == status)
	{
		atel_dbg_print("Get IP address error\n");
		return;
	}
	
	j = GET_ADDR_INFO_MIN(len, DSS_ADDR_INFO_SIZE);
	
	for (i = 0; i < j; i++)
	{
		atel_dbg_print("<--- static IP address information --->\n");
		udp_inet_ntoa(info_ptr[i].iface_addr_s, buff, DSS_ADDR_SIZE);
		atel_dbg_print("static IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		udp_inet_ntoa(info_ptr[i].gtwy_addr_s, buff, DSS_ADDR_SIZE);
		atel_dbg_print("Gateway IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		udp_inet_ntoa(info_ptr[i].dnsp_addr_s, buff, DSS_ADDR_SIZE);
		atel_dbg_print("Primary DNS IP: %s\n", buff);

		memset(buff, 0, sizeof(buff));
		udp_inet_ntoa(info_ptr[i].dnss_addr_s, buff, DSS_ADDR_SIZE);
		atel_dbg_print("Second DNS IP: %s\n", buff);
	}

	atel_dbg_print("<--- End of system info --->\n");
}

/*
@func
	tcp_set_data_param
@brief
	Set the Parameter for Data Call, such as APN and network tech.
*/
static int udp_set_data_param(void)
{
    qapi_DSS_Call_Param_Value_t param_info;
	
	/* Initial Data Call Parameter */
	memset(apn, 0, sizeof(apn));
	memset(apn_username, 0, sizeof(apn_username));
	memset(apn_passwd, 0, sizeof(apn_passwd));
	strlcpy(apn, QL_DEF_APN, QAPI_DSS_CALL_INFO_APN_MAX_LEN);

    if (NULL != udp_dss_handle)
    {
        /* set data call param */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_RADIO_TECH_UNKNOWN;	//Automatic mode(or DSS_RADIO_TECH_LTE)
        atel_dbg_print("Setting tech to Automatic\n");
        qapi_DSS_Set_Data_Call_Param(udp_dss_handle, QAPI_DSS_CALL_INFO_TECH_PREF_E, &param_info);

		/* set apn */
        param_info.buf_val = apn;
        param_info.num_val = strlen(apn);
        atel_dbg_print("Setting APN - %s\n", apn);
        qapi_DSS_Set_Data_Call_Param(udp_dss_handle, QAPI_DSS_CALL_INFO_APN_NAME_E, &param_info);
#ifdef QUEC_CUSTOM_APN
		/* set apn username */
		param_info.buf_val = apn_username;
        param_info.num_val = strlen(apn_username);
        atel_dbg_print("Setting APN USER - %s\n", apn_username);
        qapi_DSS_Set_Data_Call_Param(udp_dss_handle, QAPI_DSS_CALL_INFO_USERNAME_E, &param_info);

		/* set apn password */
		param_info.buf_val = apn_passwd;
        param_info.num_val = strlen(apn_passwd);
        atel_dbg_print("Setting APN PASSWORD - %s\n", apn_passwd);
        qapi_DSS_Set_Data_Call_Param(udp_dss_handle, QAPI_DSS_CALL_INFO_PASSWORD_E, &param_info);
#endif
		/* set IP version(IPv4 or IPv6) */
        param_info.buf_val = NULL;
        param_info.num_val = QAPI_DSS_IP_VERSION_4;
        atel_dbg_print("Setting family to IPv%d\n", param_info.num_val);
        qapi_DSS_Set_Data_Call_Param(udp_dss_handle, QAPI_DSS_CALL_INFO_IP_VERSION_E, &param_info);
    }
    else
    {
        atel_dbg_print("Dss handler is NULL!!!\n");
		return -1;
    }
	
    return 0;
}

/*
@func
	tcp_inet_ntoa
@brief
	utility interface to translate ip from "int" to x.x.x.x format.
*/
int32 udp_inet_ntoa
(
	const qapi_DSS_Addr_t    inaddr, /* IPv4 address to be converted         */
	uint8                   *buf,    /* Buffer to hold the converted address */
	int32                    buflen  /* Length of buffer                     */
)
{
	uint8 *paddr  = (uint8 *)&inaddr.addr.v4;
	int32  rc = 0;

	if ((NULL == buf) || (0 >= buflen))
	{
		rc = -1;
	}
	else
	{
		if (-1 == snprintf((char*)buf, (unsigned int)buflen, "%d.%d.%d.%d",
							paddr[0],
							paddr[1],
							paddr[2],
							paddr[3]))
		{
			rc = -1;
		}
	}

	return rc;
} /* tcp_inet_ntoa() */

/*
@func
	tcp_netctrl_init
@brief
	Initializes the DSS netctrl library for the specified operating mode.
*/
static int udp_netctrl_init(void)
{
	int ret_val = 0;
	qapi_Status_t status = QAPI_OK;

	atel_dbg_print("Initializes the DSS netctrl library\n");

	/* Initializes the DSS netctrl library */
	if (QAPI_OK == qapi_DSS_Init(QAPI_DSS_MODE_GENERAL))
	{
		udp_netctl_lib_status = DSS_LIB_STAT_SUCCESS_E;
		atel_dbg_print("qapi_DSS_Init success\n");
	}
	else
	{
		/* @Note: netctrl library has been initialized */
		udp_netctl_lib_status = DSS_LIB_STAT_FAIL_E;
		atel_dbg_print("DSS netctrl library has been initialized.\n");
	}
	
	/* Registering callback udp_net_event_cb */
	do
	{
		atel_dbg_print("Registering Callback udp_net_event_cb\n");
		
		/* Obtain data service handle */
		status = qapi_DSS_Get_Data_Srvc_Hndl(udp_net_event_cb, NULL, &udp_dss_handle);
		atel_dbg_print("udp_dss_handle %d, status %d\n", udp_dss_handle, status);
		
		if (NULL != udp_dss_handle)
		{
			atel_dbg_print("Registed udp_dss_handler success\n");
			break;
		}

		/* Obtain data service handle failure, try again after 10ms */
		qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
	} while(1);

	return ret_val;
}

/*
@func
	tcp_netctrl_start
@brief
	Start the DSS netctrl library, and startup data call.
*/
int udp_netctrl_start(void)
{
	int rc = 0;
	qapi_Status_t status = QAPI_OK;
		
	rc = udp_netctrl_init();
	if (0 == rc)
	{
		/* Get valid DSS handler and set the data call parameter */
		udp_set_data_param();
	}
	else
	{
		atel_dbg_print("quectel_dss_init fail.\n");
		return -1;
	}

	atel_dbg_print("qapi_DSS_Start_Data_Call start!!!.\n");
	status = qapi_DSS_Start_Data_Call(udp_dss_handle);
	if (QAPI_OK == status)
	{
		atel_dbg_print("Start Data service success.\n");
		return 0;
	}
	else
	{
		return -1;
	}
}

/*
@func
	tcp_netctrl_release
@brief
	Cleans up the DSS netctrl library and close data service.
*/
int udp_netctrl_stop(void)
{
	qapi_Status_t stat = QAPI_OK;
	
	if (udp_dss_handle)
	{
		stat = qapi_DSS_Stop_Data_Call(udp_dss_handle);
		if (QAPI_OK == stat)
		{
			atel_dbg_print("Stop data call success\n");
		}
	}
	
	return 0;
}	

/*
@func
	quec_dataservice_entry
@brief
	The entry of data service task.
*/
void atel_dataservice_thread(ULONG param)
{
	ULONG dss_event = 0;
	
	/* Start data call */
	udp_netctrl_start();

	while (1)
	{
		/* Wait disconnect signal */
		tx_event_flags_get(udp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR, &dss_event, TX_WAIT_FOREVER);
		if (dss_event & DSS_SIG_EVT_DIS_E)
		{
			/* Stop data call and release resource */
			udp_netctrl_stop();
			atel_dbg_print("Data service task exit.\n");
			break;
		}
	}

	atel_dbg_print("Data Service Thread Exit!\n");
	return;
}

/* begin: judge the msg type from server */
MSG_TYPE_E msg_type(char *msg)
{
	if(!strncmp(msg, "+SACK", 5))
	{
		return SACK_TYPE_E;
	}
	else if(strchr(msg, '#') || strchr(msg, '?'))
	{
		return SCMD_TYPE_E;
	}
	
	return INVALID_TYPE_E;
}
/* end: judge the msg type from server */

static int start_udp_session(void)
{
	int  sock_fd = -1;
	int  sent_len = 0;
	int  recv_len = 0;
	char buff[SENT_BUF_SIZE];
	char *p_cmd = NULL;
	MSG_TYPE_E msg_t = 0xff;
	struct sockaddr_in to;
	int32 tolen = sizeof(struct sockaddr_in);

	do
	{
		sock_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
		if (sock_fd < 0)
		{
			atel_dbg_print("Create socket error\n");			
			break;
		}
		
		atel_dbg_print("<-- Create socket[%d] success -->\n", sock_fd);
		memset(buff, 0, sizeof(buff));
		memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_port = _htons(DEF_SRV_PORT);
		to.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

		/* Connect to UDP server */
		if (-1 == qapi_connect(sock_fd, (struct sockaddr *)&to, tolen))
		{
			atel_dbg_print("Connect to servert error\n");
			break;
		}
		
		atel_dbg_print("<-- Connect to server[%s][%d] success -->\n", DEF_SRV_ADDR, DEF_SRV_PORT);

		strcpy(buff, "Hello server, This is an UDP client!");
		
		/* Start sending data to server after connecting server success */
		sent_len = qapi_sendto(sock_fd, buff, SENT_BUF_SIZE, 0, (struct sockaddr *)&to, tolen);
		if (sent_len > 0)
		{
			atel_dbg_print("Client send data success, len: %d, data: %s\n", sent_len, buff);
		}

		/* Block and wait for respons */
		while (1)
		{
			memset(buff, 0, sizeof(buff));

			/* receive with nonblock */
			recv_len = qapi_recvfrom(sock_fd, buff, SENT_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&to, &tolen);
			if (recv_len > 0)
			{
				if (0 == strncmp(buff, "Exit", 4))
				{
					qapi_socketclose(sock_fd);
					sock_fd = -1;
					tx_event_flags_set(udp_signal_handle, DSS_SIG_EVT_DIS_E, TX_OR);
					atel_dbg_print("UDP Client Exit!!!\n");
					break;
				}

				/* Reveived data */
				atel_dbg_print("[UDP Client]@len[%d], @Recv: %s\n", recv_len, buff);

				/* msg type ? */
				msg_t = msg_type(buff);

				/* +SACK msg */

				/* cmd msg */

				/* not support */

				/* allocate mem for each cmd due to asynchronous */
				if (TX_SUCCESS != tx_byte_allocate(byte_pool_udp, (VOID *)&p_cmd, strlen(buff), TX_NO_WAIT))
			  	{
			  		atel_dbg_print("tx_byte_allocate [p_cmd] failed!");
			    	//return;
			  	}

				/* show the allocate addr */
				atel_dbg_print("[start_udp_session]p_cmd addr ---> %p\n", p_cmd);
				memset(p_cmd, 0, strlen(buff));
				memcpy(p_cmd, buff, strlen(buff));

				/* enqueue the cmd */
				enqueue(p_cmd);

				/* send msg to process flow */

			}
			else //server send no cmd or SACK msg
			{
				/* receive msg from main thread */
				//tx_queue_receive(TX_QUEUE * queue_ptr, VOID * destination_ptr, ULONG wait_option);
				dequeue();
			}

			qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_MSEC, true);
		}
	} while (0);

	if (sock_fd >= 0)
	{
		qapi_socketclose(sock_fd);
	}
	
	return 0;
}

/*
@func
	quectel_task_entry
@brief
	The entry of data service task.
*/
int atel_udpclient_entry(void)
{

	int     ret = 0;
	ULONG   dss_event = 0;
	int32   sig_mask;

	/* client task start */
    atel_dbg_print("UDPClient Task Start...\n");

    /* Create a byte memory pool. */
    txm_module_object_allocate(&byte_pool_udp, sizeof(TX_BYTE_POOL));
    tx_byte_pool_create(byte_pool_udp, "udp application pool", free_memory_udp, UDP_BYTE_POOL_SIZE);

    /* Create event signal handle and clear signals */
    txm_module_object_allocate(&udp_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(udp_signal_handle, "dss_signal_event");
	tx_event_flags_set(udp_signal_handle, 0x0, TX_AND);

	/* Start DSS thread, and detect interface status */
#ifdef QAPI_TXM_MODULE
	if (TX_SUCCESS != txm_module_object_allocate((VOID *)&dss_thread_handle, sizeof(TX_THREAD))) 
	{
		return -1;
	}
#endif

	ret = tx_thread_create(dss_thread_handle, "UDPCLINET DSS Thread", atel_dataservice_thread, NULL,
							udp_dss_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS)
	{
		atel_dbg_print("Thread creation failed\n");
	}

	sig_mask = DSS_SIG_EVT_INV_E | DSS_SIG_EVT_NO_CONN_E | DSS_SIG_EVT_CONN_E | DSS_SIG_EVT_EXIT_E;
	while (1)
	{
		/* TCPClient signal process */
		tx_event_flags_get(udp_signal_handle, sig_mask, TX_OR, &dss_event, TX_WAIT_FOREVER);
		atel_dbg_print("SIGNAL EVENT IS [%d]\n", dss_event);
		
		if (dss_event & DSS_SIG_EVT_INV_E)
		{
			atel_dbg_print("DSS_SIG_EVT_INV_E Signal\n");
			tx_event_flags_set(udp_signal_handle, ~DSS_SIG_EVT_INV_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_NO_CONN_E)
		{
			atel_dbg_print("DSS_SIG_EVT_NO_CONN_E Signal\n");
			tx_event_flags_set(udp_signal_handle, ~DSS_SIG_EVT_NO_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_CONN_E)
		{
			atel_dbg_print("DSS_SIG_EVT_CONN_E Signal\n");

			/* Create a udp client and comminucate with server */
			start_udp_session();
            tx_event_flags_set(udp_signal_handle, ~DSS_SIG_EVT_CONN_E, TX_AND);
		}
		else if (dss_event & DSS_SIG_EVT_EXIT_E)
		{
			atel_dbg_print("DSS_SIG_EVT_EXIT_E Signal\n");
			tx_event_flags_set(udp_signal_handle, ~DSS_SIG_EVT_EXIT_E, TX_AND);
			tx_event_flags_delete(&udp_signal_handle);
			break;
		}
		else
		{
			atel_dbg_print("Unkonw Signal\n");
		}

		/* Clear all signals and wait next notification */
		tx_event_flags_set(udp_signal_handle, 0x0, TX_AND);	//@Fixme:maybe not need
	}
	
	atel_dbg_print("ATEL UDP Client task is Over!");
	
	return 0;
}
#endif /*__EXAMPLE_TCPCLINET__*/
/* End of Example_network.c */
