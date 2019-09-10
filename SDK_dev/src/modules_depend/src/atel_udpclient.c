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
//#include "if_server.h"
#include "atel_udpclient.h"

/*===========================================================================
                             DEFINITION
===========================================================================*/
#define QL_DEF_APN	        "CMNET"
#define DSS_ADDR_INFO_SIZE	5
#define DSS_ADDR_SIZE       16
//#define AUTO_REPORT_DEBUG


#define GET_ADDR_INFO_MIN(a, b) ((a) > (b) ? (b) : (a))


#define THREAD_STACK_SIZE    (1024 * 16)
#define THREAD_PRIORITY      (180)
#define BYTE_POOL_SIZE       (1024 * 16)

#define CMD_BUF_SIZE  100

#define DEFAULT_PUB_TIME 5

#define ACK_DEBUG
#define SELECT_TIMEOUT	10000 //timeout = 10s

extern char* g_que_first;
extern char ack_buffer[ACK_SET_LEN_MAX];
extern r_queue_s rq;
extern int auto_rep_fd;

qapi_TIMER_handle_t que_timer_handle;
qapi_TIMER_define_attr_t que_timer_def_attr;
qapi_TIMER_set_attr_t que_timer_set_attr;

#if 1
qapi_TIMER_handle_t evt_timer_handle;
qapi_TIMER_define_attr_t evt_timer_def_attr;
qapi_TIMER_set_attr_t evt_timer_set_attr;
qbool_t obs_timer_state = false;
#endif




/*===========================================================================
                           Global variable
===========================================================================*/
/* UDPClient dss thread handle */
#ifdef QAPI_TXM_MODULE
	static TX_THREAD *dss_thread_handle; 
	static TX_THREAD *udp_thread_handle; 
#else
	static TX_THREAD _dss_thread_handle;
	static TX_THREAD *ts_thread_handle = &_dss_thread_handle;
#endif

static unsigned char udp_dss_stack[THREAD_STACK_SIZE];
static unsigned char udp_recv_stack[THREAD_STACK_SIZE];


TX_EVENT_FLAGS_GROUP *udp_signal_handle;
TX_EVENT_FLAGS_GROUP *udp_recv_handle;


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

/* used to send heartbeat */
static int g_cmd_sockfd = -1;
struct sockaddr_in server;






/*===========================================================================
                               FUNCTION
===========================================================================*/
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
SER_MSG_TYPE_E msg_type(char *msg)
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


/* data(from server) process entry  */

void proc_ser_str(char * sk_buf)
{
	
	char *p_cmd = NULL;
	SER_MSG_TYPE_E msg_t = 0xff;
	
	/* msg type ? */
	msg_t = msg_type(sk_buf);
	/* go with cmd process flow */
	if(SCMD_TYPE_E == msg_t)
	{
		/* allocate mem for each cmd due to asynchronous */
		if (TX_SUCCESS != tx_byte_allocate(byte_pool_udp, (VOID *)&p_cmd, strlen(sk_buf), TX_NO_WAIT))
		{
			atel_dbg_print("tx_byte_allocate [p_cmd] failed!");
			//return;
		}

		/* show the allocate addr */
		atel_dbg_print("[proc_ser_str]p_cmd addr ---> %p\n", p_cmd);
		memset(p_cmd, 0, strlen(sk_buf));
		memcpy(p_cmd, sk_buf, strlen(sk_buf));
		
		atel_dbg_print("[p_cmd] content ---> %s\n", p_cmd);

		/* enqueue the cmd string */
		enqueue(p_cmd);
	}
	/* go with SACK process flow */
	else if(SACK_TYPE_E)
	{
		/* under development */
	}
	else 
	{
		atel_dbg_print("invalid msg from server!");
	}


	/* send msg to process flow */
}


void proc_ser_str_v2(char * sk_buf)
{
	
	char *p_cmd = sk_buf;
	SER_MSG_TYPE_E msg_t = 0xff;
	
	/* msg type ? */
	msg_t = msg_type(p_cmd);
	/* go with cmd process flow */
	if(SCMD_TYPE_E == msg_t)
	{
	
		atel_dbg_print("[proc_ser_str_v2]sk_buf addr ---> %p\n", p_cmd);

		/* parse sk_buff */
		cmd_parse_entry(p_cmd);
	}
	/* go with SACK process flow */
	else if(SACK_TYPE_E == msg_t)
	{
		/* under development */
	}
	else 
	{
		atel_dbg_print("invalid msg from server!");
	}


	/* send msg to process flow */
}

void que_cb_timer()
{
	char *buff = "keep connection alive";
	int sent_len = -1;
	/* keep connection alive */
	sent_len = qapi_sendto(g_cmd_sockfd, buff, strlen(buff), 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
	if(sent_len > 0)
	{
		atel_dbg_print("[keep connection], len: %d, data: %s\n", sent_len, buff);
	}
	
	return;
}

void cmd_proc_timer_init(void)
{
	int status;

	memset(&que_timer_def_attr, 0, sizeof(que_timer_def_attr));
	que_timer_def_attr.cb_type	= QAPI_TIMER_FUNC1_CB_TYPE;
	que_timer_def_attr.deferrable = false;
	que_timer_def_attr.sigs_func_ptr = que_cb_timer;
	que_timer_def_attr.sigs_mask_data = 0x11;
	status = qapi_Timer_Def(&que_timer_handle, &que_timer_def_attr);
	atel_dbg_print("[TIMER] status[%d]", status);

	memset(&que_timer_set_attr, 0, sizeof(que_timer_set_attr));
	que_timer_set_attr.reload = false;
	que_timer_set_attr.time = 2;
	que_timer_set_attr.unit = QAPI_TIMER_UNIT_MIN;
	status = qapi_Timer_Set(que_timer_handle, &que_timer_set_attr);
}


/*
@func
	quec_dataservice_entry
@brief
	The entry of data service task.
*/
void udp_receive_thread(ULONG param)
{
	ULONG udp_event = 0;	
	bool parse_state = FALSE;	
	char recv_buff[ACK_SET_LEN_MAX];
	int recv_len = -1;
	int send_len = -1;
	int32 tolen = sizeof(struct sockaddr_in);
	

	while (1)
	{
	
		atel_dbg_print("[udp_receive_thread]---wait udp receive signal---");
		/* Wait disconnect signal */
		tx_event_flags_get(udp_recv_handle, UDP_SIG_EVT_RECV_E, TX_AND_CLEAR, &udp_event, TX_WAIT_FOREVER);
		
		atel_dbg_print("[udp_receive_thread]---get udp receive signal---");
		
		if (udp_event & UDP_SIG_EVT_RECV_E)
		{
			
			memset(recv_buff, 0, sizeof(recv_buff));

			/* receive with nonblock */
			recv_len = qapi_recvfrom(g_cmd_sockfd, recv_buff, ACK_SET_LEN_MAX, 0, (struct sockaddr *)&server, &tolen);
			
			/* Reveived data */
			atel_dbg_print("[UDP Client]@len[%d], @Recv: %s\n", recv_len, recv_buff);
			if (recv_len > 0)
			{
				proc_ser_str_v2(recv_buff);
				parse_state = TRUE;
			}
						
			/* move the flow only when parse state is set */
			if(parse_state == TRUE)
			{
				atel_dbg_print("[udp_receive_thread]ack_buffer[%p, %s]\n", ack_buffer, ack_buffer);

				/* send ack to server */
				send_len = qapi_sendto(g_cmd_sockfd, ack_buffer, strlen(ack_buffer), 0, (struct sockaddr *)&server, tolen);
				
				if (send_len > 0)
				{
					atel_dbg_print("Client send ack success, len: %d, data: %s\n", send_len, ack_buffer);

					/* clear ack buffer */
					memset(ack_buffer, 0, sizeof(ack_buffer));
				}

				/* clear the parse state */
				parse_state = FALSE;
			}
		}
	}

	atel_dbg_print("udp_receive_thread exit!");
	return;
}


static int start_udp_session(void)
{
	int  sock_fd = -1;
	int  sent_len = 0;
	int  recv_len = 0;
	int  ret = 0xff;
	int32   sig_mask;
	uint8 hb_cnt = 0;
	char send_buff[SENT_BUF_SIZE] = {0};
	char recv_buff[RECV_BUF_SIZE] = {0};
	//char recv_buff[SENT_BUF_SIZE];	
	bool parse_state = FALSE;
    fd_set  fset;
	
	struct sockaddr_in to;
	
	struct sockaddr_in client_addr;
	int32 tolen = sizeof(struct sockaddr_in);

	
    /* Create event signal handle and clear signals */
    txm_module_object_allocate(&udp_recv_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(udp_recv_handle, "udp receive process thread");
	tx_event_flags_set(udp_recv_handle, 0x0, TX_AND);
	
	sig_mask = UDP_SIG_EVT_RECV_E;

	do
	{
		#if 0
		if (TX_SUCCESS != txm_module_object_allocate((VOID *)&udp_thread_handle, sizeof(TX_THREAD))) 
		{
			return -1;
		}

		/* begin: start sub thread for receive data from server */
		ret = tx_thread_create(udp_thread_handle, "udp client receive thread", udp_receive_thread, NULL,
								udp_recv_stack, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
		if (ret != TX_SUCCESS)
		{
			atel_dbg_print("Thread creation failed\n");
		}
		/* end */
		#endif
	
		//descripter for cmd process
		sock_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
		if (sock_fd < 0)
		{
			atel_dbg_print("Create socket error\n");			
			break;
		}

		auto_rep_fd = sock_fd;
		
		/* socket for msg auto report */
#if 0
		auto_rep_fd = qapi_socket(AF_INET, DEF_SRC_TYPE, 0);
		if (auto_rep_fd < 0)
		{
			atel_dbg_print("auto report socket create failed!");			
			break;
		}
#endif
		atel_dbg_print("<-- Create socket[%d] success -->", sock_fd);
		memset(send_buff, 0, sizeof(send_buff));
		memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_port = _htons(DEF_SRV_PORT);
		to.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);
		
		atel_dbg_print("<-- Connect to server[%s][%d] success -->", DEF_SRV_ADDR, DEF_SRV_PORT);

		strcpy(send_buff, "Hello server, This is an udp client!");
		
		/* Start sending data to server after connecting server success */
		sent_len = qapi_sendto(sock_fd, send_buff, strlen(send_buff), 0, (struct sockaddr *)&to, tolen);
		if (sent_len > 0)
		{
			atel_dbg_print("Client send data success, len: %d, data: %s", sent_len, send_buff);
		}


		/* I/O multiplexing */
		while (1)
		{
			atel_dbg_print("<---wait data from udp server--->");
			#if 1
			qapi_fd_zero(&fset);
			qapi_fd_set(sock_fd, &fset);
			ret = qapi_select(&fset, NULL, NULL, SELECT_TIMEOUT);
			if(ret > 0)
			{

				atel_dbg_print("---have data in read set---");
				/* IO data for read */
				if(FD_IS_MEM_E == qapi_fd_isset(sock_fd, &fset))
				{
					#if 0
					atel_dbg_print("---udp receive signal triggered---");
					#ifdef UDP_RECV_DBG
					g_cmd_sockfd = sock_fd;
					memcpy(&server, &to, sizeof(struct sockaddr_in));
					
					/* send event to sub thread to process received data */
					tx_event_flags_set(udp_recv_handle, UDP_SIG_EVT_RECV_E, TX_OR);
					
					atel_dbg_print("[start_udp_session]---udp receive ready---");
					#endif
					#else 
					//atel_dbg_print("[start_udp_session]---start process received data---");
					memset(recv_buff, 0, sizeof(recv_buff));
					recv_len = qapi_recvfrom(sock_fd, recv_buff, RECV_BUF_SIZE, 0, (struct sockaddr*)&to, &tolen);
					
					if (recv_len > 0)
					{
						proc_ser_str_v2(recv_buff);
						parse_state = TRUE;
					}
								
					/* move the flow only when parse state is set */
					if(parse_state == TRUE)
					{
						atel_dbg_print("[udp_receive_thread]ack_buffer[%p, %s]\n", ack_buffer, ack_buffer);
					
						/* send ack to server */
						sent_len = qapi_sendto(sock_fd, ack_buffer, strlen(ack_buffer), 0, (struct sockaddr *)&to, tolen);
						
						if (sent_len > 0)
						{
							atel_dbg_print("Client send ack success, len: %d, data: %s\n", sent_len, ack_buffer);
					
							/* clear ack buffer */
							memset(ack_buffer, 0, sizeof(ack_buffer));
						}
					
						/* clear the parse state */
						parse_state = FALSE;
					}

					#endif
					
					#if 0 
					if (recv_len > 0)
					{

						/* Reveive data and response*/
						atel_dbg_print("[UDP Client]@len[%d], @Recv: %s\n", recv_len, recv_buff);

						memset(send_buff, 0, SENT_BUF_SIZE);
						snprintf(send_buff, SENT_BUF_SIZE, "[UDP Server]received %d bytes data", recv_len);
						sent_len = qapi_sendto(sock_fd, send_buff, strlen(send_buff), 0, (struct sockaddr*)&to, tolen);
						if(sent_len < 0)
						{
							atel_dbg_print("UDP Server send data failed: %d\n", sent_len);
						}
					}
					#endif
				}
				else
				{
					atel_dbg_print("---cur sock_fd not in fset---");
				}
			}
			else if(ret == 0)
			{
				atel_dbg_print("---opps, select timeout, [hb_cnt]%d---", hb_cnt);
				
				/* send heart beat every 20 seconds */
				if(hb_cnt++ == 2)
				{
					atel_dbg_print("---send heartbeat---");
					sent_len = qapi_sendto(sock_fd, "heartbeat", strlen("heartbeat"), 0, (struct sockaddr *)&to, tolen);
					
					if (sent_len > 0)
					{
						atel_dbg_print("Client send heartbeat, len: %d, data: %s\n", sent_len, "heartbeat");
					}

#if 0
					/* heart beat for auto report */
					sent_len = qapi_sendto(auto_rep_fd, "heartbeat for auto report", strlen("heartbeat for auto report"), 0, (struct sockaddr *)&to, tolen);
					
					if (sent_len > 0)
					{
						atel_dbg_print("Client send heartbeat, len: %d, data: %s\n", sent_len, "heartbeat for auto report");
					}
#endif
				
					hb_cnt = 0;
				}
			}
			
		
			#endif

			qapi_Timer_Sleep(1000, QAPI_TIMER_UNIT_MSEC, true);
		}
	} while (0);

	if (sock_fd >= 0)
	{
		qapi_socketclose(sock_fd);
	}
	
	return 0;
}

#if 1
/* data notification callback */
static void udp_data_notify_signal_callback(uint32 userData)
{
	int sent_len = 0;
	int sock_fd = -1;
	static uint32 cnt = 0;
	char sent_buff[SENT_BUF_SIZE];
	struct sockaddr_in server_addr;
	char *fir_ele = NULL;

	sock_fd = auto_rep_fd;
	memset(sent_buff, 0, sizeof(sent_buff));
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = _htons(DEF_SRV_PORT);
	server_addr.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

#if 1
	/* get head addr of the queue */
	fir_ele = get_first_ele();
	atel_dbg_print("addr of first element is [%p]", fir_ele);
	strncpy(sent_buff, fir_ele, strlen(fir_ele));
	/* release head element */
	dequeue();
#else
	strcpy(sent_buff, "this msg came from data transmit timer");
#endif

	/* Start sending data to server after connecting server success */
	sent_len = qapi_sendto(sock_fd, sent_buff, strlen(sent_buff), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

	if (sent_len > 0)
	{
		cnt++;
		atel_dbg_print("Client send data success, len: %d, cnt: %d\n", sent_len, cnt);
	}
	else
	{
		atel_dbg_print("Send data failed: %d\n", sent_len);
	}

  	return;
}


void udp_data_transfer_timer_def(void)
{
	//if (!obs_timer_state)
	{
		
	    evt_timer_def_attr.deferrable = false;
	    evt_timer_def_attr.cb_type = QAPI_TIMER_FUNC1_CB_TYPE;
	    evt_timer_def_attr.sigs_func_ptr = udp_data_notify_signal_callback;
	    evt_timer_def_attr.sigs_mask_data = 0;

		qapi_Timer_Def(&evt_timer_handle, &evt_timer_def_attr);
	}
}

void udp_data_transfer_timer_start()
{
	//qapi_TIMER_set_attr_t time_attr;
	evt_timer_set_attr.unit = QAPI_TIMER_UNIT_SEC;
	evt_timer_set_attr.reload = 5;
	evt_timer_set_attr.time = 5; /* 5 sec */
	
	qapi_Timer_Set(evt_timer_handle, &evt_timer_set_attr);
	obs_timer_state = true;
}

static void udp_data_transfer_timer_stop(void)
{
	if (obs_timer_state)
	{
		obs_timer_state = false;
		qapi_Timer_Stop(evt_timer_handle);
		qapi_Timer_Undef(evt_timer_handle);
	}
}
#endif

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
    atel_dbg_print("udp client thread Start...\n");

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
#if 0	
	/* Init timer for sending data every 5seconds */
	udp_data_transfer_timer_def();
	udp_data_transfer_timer_start();
#endif

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
