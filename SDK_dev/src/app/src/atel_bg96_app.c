/******************************************************************************
*@file    atel_bg96_app.c
*@brief   example of atel BG96 application
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__ATEL_BG96_APP__)
#include "txm_module.h"
#include "qapi_diag.h"
#include "qapi_timer.h"
#include "qapi_uart.h"
#include "qapi_atfwd.h"
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "atel_bg96_app.h"
#include "events_if.h"
#include "atel_tcpclient.h"
#include "atel_gps.h"
#include <locale.h>






/**************************************************************************
*                                 DEFINE
***************************************************************************/
#define ATEL_DEBUG
#define IF_SERVER_DEBUG
#define AUTO_REPORT_DEBUG
#define EVENTS_REPORT_DEBUG


/* begin: queue for communicate */
TX_QUEUE *evt_notice;
TX_MUTEX *rep_mem_mutex;

/* end */


TX_TIMER *warning_intr;
TX_TIMER *warning_copy_scheduler;
TX_TIMER *unsafe_intr;
TX_TIMER *unsafe_copy_scheduler;

TX_TIMER *stats_timer;

/* var for statistic */
ULONG num_unsafe = 0;
ULONG num_warning = 0;


qapi_TIMER_handle_t timer_handle;
qapi_TIMER_define_attr_t timer_def_attr;
qapi_TIMER_set_attr_t timer_set_attr;

#if 0
qapi_TIMER_handle_t evt_timer_handle;
qapi_TIMER_define_attr_t evt_timer_def_attr;
qapi_TIMER_set_attr_t evt_timer_set_attr;
qbool_t obs_timer_state = false;
#endif

#define ATEL_IG_ON_RUNNING_MODE_CFG_FILE		"/datatx/ig_mode_cfg"
/**************************************************************************
*                                 GLOBAL
***************************************************************************/

TX_TIMER gpio_sim = {0};

const char* p_msg_rep = "this is gps data";


#define LOC_SAVE_OK		0
#define LOC_SAVE_ERR	-1

#define MAX_TMP_MEM 200

ULONG frame_index = 0, event_count = 0, frame_data[2];
ULONG tmp_mem[MAX_TMP_MEM][2];
ULONG rep_buf[MAX_EVENTS][26];




/* begin: MSO feature */
#define MSO_COUNTER_DEFAULT	5
static char g_mso_counter = 0;
/* end */

/* begin: sim relay from ble */
boolean g_sim_relay_ble = LOW;
boolean g_timer_switch = FALSE;
static boolean g_sim_counter = 0;
/* end */

static int main_task_run_cnt = 0;
int auto_rep_fd = -1;

/* timer for daily report */
TX_TIMER *daily_rep = NULL;
#define DFT_INTEVAL_TICK 10000 //auto report interval(30 seconds) after system up
boolean g_daily_timer_restart = FALSE;

/* system timer init */
TX_TIMER auto_report; /* for auto report event */
TX_TIMER ig_on_event; /* for ignition on event */
TX_TIMER ig_off_event; /* for ignition off event */

#define EXPIRE_TICK		12


//static TX_EVENT_FLAGS_GROUP gps_file_handle;

/* begin: for ignition status switch signal */
TX_EVENT_FLAGS_GROUP *ig_switch_signal_handle;

/* end */

/* begin: for events detect */
TX_EVENT_FLAGS_GROUP *events_signal_handle;
TX_BYTE_POOL *byte_pool_event;
#define EVENT_POOL_START 	(void*)0x500000
#define FREE_MEM_SIZE		8*32*1024 //2M mem for msg process
//const void * event_pool_start = EVENT_POOL_START;
char event_pool_start[FREE_MEM_SIZE];


void* event_que_handle = NULL;
/* end */

extern ULONG auto_intval_last;


static boolean normal_start = 0;

/* Modify this pin num which you want to test */
MODULE_PIN_ENUM  g_gpio_pin_num = PIN_E_GPIO_20;


subtask_config_t tcpclient_task_config ={
    
    NULL, "Atel Tcpclient Task Thread", atel_tcpclient_entry, \
    atel_tcpclient_thread_stack, ATEL_TCPCLIENT_THREAD_STACK_SIZE, ATEL_TCPCLIENT_THREAD_PRIORITY
    
};

subtask_config_t udpclient_task_config ={
    
    NULL, "Atel Udpclient Task Thread", atel_udpclient_entry, \
    atel_udpclient_thread_stack, ATEL_UDPCLIENT_THREAD_STACK_SIZE, ATEL_UDPCLIENT_THREAD_PRIORITY
    
};

	
subtask_config_t gps_task_config ={
	
	NULL, "Atel GPS Task Thread", atel_gps_entry, \
	atel_gps_thread_stack, ATEL_GPS_THREAD_STACK_SIZE, ATEL_GPS_THREAD_PRIORITY
	
};

void report_entry(void)
{
	return;
}


subtask_config_t evt_cap_config ={
	
	NULL, "Atel data capture thread", data_capture_process, \
	atel_event_cap_thread_stack, ATEL_EVENT_CAP_THREAD_STACK_SIZE, ATEL_EVENT_CAP_THREAD_PRIORITY
	
};


subtask_config_t evt_rep_config ={
	
	NULL, "Atel event report thread", event_record, \
	atel_event_rep_thread_stack, ATEL_EVENT_REP_THREAD_STACK_SIZE, ATEL_EVENT_REP_THREAD_PRIORITY
	
};


subtask_config_t mdm_ble_at_config ={
	
	NULL, "Atel MDM_COM_BLE Task Thread", atel_mdm_ble_entry, \
	atel_at_frame_thread_stack, ATEL_AT_FRAME_THREAD_STACK_SIZE, ATEL_AT_FRAME_THREAD_PRIORITY
	
};



/*
@func
  atel_dbg_print
@brief
  Output the debug log to main uart. 
*/
void atel_dbg_print(const char* fmt, ...)
{
	char log_buf[256] = {0};

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
	va_end(ap);

	qapi_atfwd_send_urc_resp("ATEL", log_buf);
	//qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_MSEC, true);

	return;
}

void atel_event_flag_set(MSG_TYPE_REL_E evt_bit)
{
	tx_event_flags_set(events_signal_handle, evt_bit, TX_OR);

	return;
}

void atel_event_flag_clr(MSG_TYPE_REL_E evt_bit)
{
	tx_event_flags_set(events_signal_handle, ~evt_bit, TX_AND);

	return;
}

void atel_event_flag_get_with_clr(MSG_TYPE_REL_E evt_bit, ULONG events)
{
	tx_event_flags_get(events_signal_handle, evt_bit, TX_AND_CLEAR, &events, TX_WAIT_FOREVER);

	return;
}



/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/

/*
@func
  location_info_show
@brief
  Show location info.
*/
void gps_cb_timer(uint32_t data)
{
    if(0x11 == data) /* specified when define attr of timer */
    {
    	/* update gps loc */
    }
}


/*
@func
  location_info_show
@brief
  Show location info.
*/

void gps_loc_timer_init()
{
	int status = 0xff;
	memset(&timer_def_attr, 0, sizeof(timer_def_attr));
	timer_def_attr.cb_type	= QAPI_TIMER_FUNC1_CB_TYPE;
	timer_def_attr.deferrable = false;
	timer_def_attr.sigs_func_ptr = gps_cb_timer;
	timer_def_attr.sigs_mask_data = 0x11;
	status = qapi_Timer_Def(&timer_handle, &timer_def_attr);
	
#ifdef ATEL_DEBUG
	atel_dbg_print("[TIMER] status[%d]", status);
#endif

	memset(&timer_set_attr, 0, sizeof(timer_set_attr));
	timer_set_attr.reload = true; /* timer will reactive once expires */
	timer_set_attr.time = 60; /* frequency we update the gps location */
	timer_set_attr.unit = QAPI_TIMER_UNIT_SEC;
	status = qapi_Timer_Set(timer_handle, &timer_set_attr);

#ifdef ATEL_DEBUG
	atel_dbg_print("[TIMER] status[%d]", status);
#endif
}


int ota_service_start(void)
{
	return 0;
}


char tcp_service_start(void)
{
    int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&tcpclient_task_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return - 1;
	}

	/* create the subtask step by step */
	status = tx_thread_create(tcpclient_task_config.module_thread_handle,
					   tcpclient_task_config.module_task_name,
					   tcpclient_task_config.module_task_entry,
					   NULL,
					   tcpclient_task_config.module_thread_stack,
					   tcpclient_task_config.stack_size,
					   tcpclient_task_config.stack_prior,
					   tcpclient_task_config.stack_prior,
					   TX_NO_TIME_SLICE,
					   TX_AUTO_START
					   );
	  
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[task_create] : Thread creation failed");
	}

	return 0;
}

char udp_service_start(void)
{
    int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&udpclient_task_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return -1;
	}

	/* create the subtask step by step */
	status = tx_thread_create(udpclient_task_config.module_thread_handle,
					   udpclient_task_config.module_task_name,
					   udpclient_task_config.module_task_entry,
					   NULL,
					   udpclient_task_config.module_thread_stack,
					   udpclient_task_config.stack_size,
					   udpclient_task_config.stack_prior,
					   udpclient_task_config.stack_prior,
					   TX_NO_TIME_SLICE,
					   TX_AUTO_START
					   );
	  
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[task_create] : Thread creation failed");
		return -1;
	}

	return 0;
}


char gps_service_start(void)
{
	int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&gps_task_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return -1;
	}

	/* create the subtask step by step */
	status = tx_thread_create(gps_task_config.module_thread_handle,
					   gps_task_config.module_task_name,
					   gps_task_config.module_task_entry,
					   NULL,
					   gps_task_config.module_thread_stack,
					   gps_task_config.stack_size,
					   gps_task_config.stack_prior,
					   gps_task_config.stack_prior,
					   TX_NO_TIME_SLICE,
					   TX_AUTO_START
					   );
	  
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[task_create] : Thread creation failed");
		return -1;
	}

	return 0;
}

char event_capture_service_start(void)
{
	int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&evt_cap_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return -1;
	}

	/* create the subtask step by step */
	status = tx_thread_create(evt_cap_config.module_thread_handle,
					   evt_cap_config.module_task_name,
					   evt_cap_config.module_task_entry,
					   NULL,
					   evt_cap_config.module_thread_stack,
					   evt_cap_config.stack_size,
					   evt_cap_config.stack_prior,
					   evt_cap_config.stack_prior,
					   TX_NO_TIME_SLICE,
					   TX_AUTO_START
					   );
	  
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[task_create] : Thread creation failed");
		return -1;
	}

	return 0;
}


char event_report_service_start(void)
{
	int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&evt_rep_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return -1;
	}

	/* create the subtask step by step */
	status = tx_thread_create(evt_rep_config.module_thread_handle,
					   evt_rep_config.module_task_name,
					   evt_rep_config.module_task_entry,
					   NULL,
					   evt_rep_config.module_thread_stack,
					   evt_rep_config.stack_size,
					   evt_rep_config.stack_prior,
					   evt_rep_config.stack_prior,
					   TX_NO_TIME_SLICE,
					   TX_DONT_START /* triggered by event detect timer */
					   );
	  
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[task_create] : Thread creation failed");
		return -1;
	}

	return 0;
}


int bg96_com_ble_task(void)
{
	
	int32 status = -1;

	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&mdm_ble_at_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return - 1;
	}

	/* create the subtask step by step */
	status = tx_thread_create(mdm_ble_at_config.module_thread_handle,
					   mdm_ble_at_config.module_task_name,
					   mdm_ble_at_config.module_task_entry,
					   NULL,
					   mdm_ble_at_config.module_thread_stack,
					   mdm_ble_at_config.stack_size,
					   mdm_ble_at_config.stack_prior,
					   mdm_ble_at_config.stack_prior,
					   TX_NO_TIME_SLICE,
					   TX_AUTO_START
					   );
	  
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[task_create] : Thread creation failed");
		return -1;
	}

	return 0;
}


void cb_timer(ULONG gpio_phy)
{   
#if 0
	g_sim_counter++;
	atel_dbg_print("[cb_timer]g_sim_counter is: %d", g_sim_counter);
	if(g_sim_counter % 2)
	{
		g_sim_relay_ble = HIGH;
	}
	else 
	{
		g_sim_relay_ble = LOW;
	}

#ifdef IG_INT_REPORT
	/* ignition status detect */
	if(HIGH == get_ig_status())
	{
		g_sim_relay_ble = HIGH;
	}
	else 
	{
		g_sim_relay_ble = LOW;
	}
#endif
#endif

#if 0
#ifdef ATEL_DEBUG
	static ULONG tri_cnt = 0;
	int sent_len = -1;
	char sent_buf[256];
	struct sockaddr_in to;
	int32 tolen = sizeof(struct sockaddr_in);
	/* use udp socket to report msg */
	
#ifdef ATEL_DEBUG
	//memset(send_buff, 0, sizeof(send_buff));
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = _htons(DEF_SRV_PORT);
	to.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

#endif


	atel_dbg_print("<-- socket:%d, send msg to udp server, the %d time-->\n", auto_rep_fd, ++tri_cnt);
#if 1

	/* send daily report to udp server with fixed interval */
	memset(sent_buf, 0, sizeof(sent_buf));
	strcpy(sent_buf, "hello server, this is gps data!");
	sent_len = qapi_sendto(auto_rep_fd, sent_buf, strlen(sent_buf), 0, (struct sockaddr *)&to, tolen);
	//sent_len = qapi_send(auto_rep_fd, sent_buf, strlen(sent_buf), 0);
	if (sent_len > 0)
	{
		atel_dbg_print("daily report timer send data success, len: %d, data: %s\n", sent_len, sent_buf);
	}
#endif

#endif
#endif

	
	return;
}



void ig_timer_init(void)
{
	int status;

	memset(&timer_def_attr, 0, sizeof(timer_def_attr));
	timer_def_attr.cb_type	= QAPI_TIMER_FUNC1_CB_TYPE;
	timer_def_attr.deferrable = false;
	timer_def_attr.sigs_func_ptr = cb_timer;
	timer_def_attr.sigs_mask_data = 0x11;
	status = qapi_Timer_Def(&timer_handle, &timer_def_attr);
	//atel_dbg_print("[TIMER] status[%d]", status);

	memset(&timer_set_attr, 0, sizeof(timer_set_attr));
	timer_set_attr.reload = 10;//reload inteval, periodicly triggered
	timer_set_attr.time = 10;//first time that expire, only once
	timer_set_attr.unit = QAPI_TIMER_UNIT_SEC;
	status = qapi_Timer_Set(timer_handle, &timer_set_attr);

}

#if 0
/* data notification callback */
void udp_data_notify_signal_callback(uint32 userData)
{
	int sent_len = 0;
	int sock_fd = -1;
	static uint32 cnt = 0;
	char sent_buff[SENT_BUF_SIZE];
	struct sockaddr_in server_addr;

	sock_fd = auto_rep_fd;
	memset(sent_buff, 0, sizeof(sent_buff));
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = _htons(DEF_SRV_PORT);
	server_addr.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

	strcpy(sent_buff, "this msg came from data transmit timer");

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
  relay_from_ble
@brief
  get relay status from ble. 
*/
/*=========================================================================*/
boolean relay_from_ble(void)
{	
	return g_sim_relay_ble;
}

void report_func(ULONG timer_input)
{
	ULONG upt_intval = 0xff;
	static ULONG tri_cnt = 0;
	int sent_len = -1;
	struct sockaddr_in to;
	int32 tolen = sizeof(struct sockaddr_in);
	/* use udp socket to report msg */

	
	atel_dbg_print("<-- socket:%d, send msg to udp server, the %d time-->\n", auto_rep_fd, ++tri_cnt);
	#if 1
	//memset(send_buff, 0, sizeof(send_buff));
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = _htons(DEF_SRV_PORT);
	to.sin_addr.s_addr = inet_addr(DEF_SRV_ADDR);

	/* send daily report to udp server with fixed interval */
	sent_len = qapi_sendto(auto_rep_fd, p_msg_rep, strlen(p_msg_rep), 0, (struct sockaddr *)&to, tolen);
	if (sent_len > 0)
	{
		atel_dbg_print("daily report timer send data success, len: %d, data: %s\n", sent_len, p_msg_rep);
	}
	#endif

	/* auto report intval had been update */
	if(g_daily_timer_restart == TRUE)
	{
		
		upt_intval = auto_intval_last;
		tx_timer_deactivate(&daily_rep);
		
		/* make sure deactive is finished, may not necessary */
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_TICK, true);
		
		/* change init and reload time */
		tx_timer_change(&daily_rep, upt_intval, upt_intval);
		
		/* restart timer */
		tx_timer_activate(&daily_rep);
		/* clear flag */
		g_daily_timer_restart = FALSE;
	}
}

/* func: daily heart beat report 
*/
char daily_report_start(void)
{
	ULONG status = 0xff;
	ULONG cur_intval = 0xff;

	status = txm_module_object_allocate(&daily_rep, sizeof(TX_TIMER));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[daily_report_start] txm_module_object_allocate daily_rep failed, %d", status);
	}

	/* timer init */
	cur_intval = auto_intval_last;
	atel_dbg_print("<------cur_intval is %d------>", cur_intval);
	/* create auto report timer with no start */
	status = tx_timer_create(daily_rep, "daily report", report_func, 0, DFT_INTEVAL_TICK, cur_intval, TX_AUTO_ACTIVATE);
	if(TX_SUCCESS != status)
	{
		atel_dbg_print("[daily_report] tx_timer_create failed!, status:%x", status);
		return -1;
	}
	
	return 0;
}


void event_related_timer_init()
{
	/* auto report timer */

	/* ignition off/on timer */

	/*  */
	return;
}





boolean system_init(void)
{
	int status = 0xff;
	/* tcp service bring up */
	//tcp_service_start();

	/* udp service start */
	status = udp_service_start();
	if(status != 0)
	{
		atel_dbg_print("udp service invalid!");
	}
	
	/* gps service start */
    status = gps_service_start();
	if(status != 0)
	{
		atel_dbg_print("gps service invalid!");
	}

#ifndef EVENTS_REPORT_DEBUG
	/* event report service start */
    status = event_capture_service_start();
	if(status != 0)
	{
		atel_dbg_print("event_capture_service_start service invalid!");
	}


	/* event report service start */
    status = event_report_service_start();
	if(status != 0)
	{
		atel_dbg_print("event_report_sevice_start service invalid!");
	}

#endif

	#if 0
	/* daily heart beat report after system up */
	status = daily_report_start();
	if(status != 0)
	{
		atel_dbg_print("daily report start failed!");
	}
	#endif
	
	return status? FALSE : TRUE;
}

char vehicle_ignition_detect(void)
{
	int sig_mask = IG_SWITCH_OFF_E | IG_SWITCH_ON_E;
	ULONG switch_event = 0;
	boolean ig_status = 0xff;
	
	/* DO1(relay) status check */
	//ig_status = relay_from_ble();

	/* get status from  */
	tx_event_flags_get(&ig_switch_signal_handle, sig_mask, TX_OR, &switch_event, TX_NO_WAIT);
	
	/* swtich from off to on */
	if (switch_event&IG_SWITCH_ON_E)
	{	
		/* clear the switch on bit */
		
		return SWITCH_ON;
	}
	/* switch from on to off */
	else if (switch_event&IG_SWITCH_OFF_E)
	{
		/* clear the switch off bit */
		
		return SWITCH_OFF;
	}
	return ig_status;
}

void gps_loc_timer_start()
{
	return;
}


void loc_report(boolean    update_flag)
{
	/* get loc info from gps module */

	/* loc to TCP client's normal queue */
	//qapi_send(int32_t handle, char * buf, int32_t len, int32_t flags);

	if(update_flag == TRUE)
	{
		/* start the timer to update the loc */
		gps_loc_timer_init();
	}
}


IG_ON_RUNNING_MODE_E check_running_mode(char *cfg_path)
{
	int fd = -1;
	int status = 0xff;
	char file_buff[10] = {0};
	uint32 rd_bytes = 0;
	
	qapi_FS_Open(cfg_path, QAPI_FS_O_RDONLY_E , &fd);
	status = qapi_FS_Read(fd, (uint8*)file_buff, sizeof(file_buff), &rd_bytes);
	/* watch mode */
	if (strncasecmp(file_buff, "watch", rd_bytes) == 0)
	{
		return IG_ON_WATCH_M;
	}
	else if (strncasecmp(file_buff, "theft", rd_bytes) == 0)
	{
		return IG_ON_THEFT_M;
	}
	/* otherwise */
	else
	{
		/* use the default mode */
		return IG_ON_DEFAULT_M;
	}
	
	return IG_ON_INVALID_M;
}

void MSO_enable(void)
{
	/* init mso counter */
    g_mso_counter = MSO_COUNTER_DEFAULT;

	
	return;
}

void ig_off_process()
{
	
	char *ig_timer_name = NULL;
	UINT active = 0xff;
	ULONG remaining_ticks = 0;
	ULONG reschedule_ticks = 0;
	UINT status = 0;
    uint32 message_size;
	//TASK_COMM qt_main_task_comm;

	
	#if 0
	/* whether enable MSO feature or not */
	if(motion_status && cell_coverage && gps_fix_valid)
	{
		/* report gps location */
		loc_report(FALSE);

		/* detect whether ble disable relay */
		if(OK == relay_from_ble())
		{
			MSO_enable();
		}
	}
	#endif

	/* ig off flow loop */
	while(1)
	{
		/* report event or alarm, condition??? */
		alarm_event_process();
		
		/* send signal to main task when ignition status switched */
		if (HIGH == relay_from_ble())
		{
			atel_dbg_print("[ig_off_process]off ---> on!");
			tx_event_flags_set(ig_switch_signal_handle, IG_SWITCH_ON_E, TX_OR);
			
			/* terminate ig off data flow */
			break;
		}


		/* begin: got msg from udp client */
		#ifdef MSG_QUEUE_DBG
		memset(&qt_main_task_comm, 0, sizeof(qt_main_task_comm));

		/* rec msg from udp client task */
		status = tx_queue_receive(tx_queue_handle, &qt_main_task_comm, TX_WAIT_FOREVER);
		
		if(TX_SUCCESS != status)
		{
			atel_dbg_print("[ig_off_process] tx_queue_receive failed with status %d", status);
		}
		else
		{
		   atel_dbg_print("[ig_off_process]tx_queue_receive ok with status %d", status);
		}
		#endif
		/* end: got msg from udp client */
		
		qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	}
	
	return ;
}

void drv_behav_detect(void)
{
	return;
}




void ig_on_process(IG_ON_RUNNING_MODE_E mode)
{

	/* loc report and update base on the running mode */
	//loc_report(boolean update_flag);

	
	char *ig_timer_name = NULL;
	UINT active = 0xff;
	ULONG remaining_ticks = 0;
	ULONG reschedule_ticks = 0;
	TX_TIMER * next_timer;

	/* ignition on event is triggered */
	tx_event_flags_set(events_signal_handle, IG_ON_EVT, TX_OR);

	while(1)
	{
		/* driving behavior detect */
		drv_behav_detect();
		
		/* set alarm or event bit if condition met */

		/* report event or alarm, condition??? */
		alarm_event_process();
		
		/* send signal to main task when ignition status switch to ignition Off */
		if (LOW == relay_from_ble())
		{
			atel_dbg_print("[ig_on_process]on ---> off!");
			tx_event_flags_set(ig_switch_signal_handle, IG_SWITCH_OFF_E, TX_OR);
			break;
		}
 
		
		
		qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	}

	return;
}




void gpio_init(void)
{
	//config gpio direction
	gpio_dir_control(PIN_E_GPIO_01,QAPI_GPIO_INPUT_E);  //charge_DET
	gpio_dir_control(PIN_E_GPIO_02,QAPI_GPIO_OUTPUT_E); //MDM_LDO_EN
	gpio_dir_control(PIN_E_GPIO_04,QAPI_GPIO_OUTPUT_E); //LNA_Power_EN
	gpio_dir_control(PIN_E_GPIO_05,QAPI_GPIO_INPUT_E);  //WAKEUP_IN
	gpio_dir_control(PIN_E_GPIO_09,QAPI_GPIO_OUTPUT_E); //WAKE_BLE
	gpio_dir_control(PIN_E_GPIO_19,QAPI_GPIO_OUTPUT_E); //GREEN_LED
	gpio_dir_control(PIN_E_GPIO_20,QAPI_GPIO_OUTPUT_E); //BLUE_LED
	//gpio_dir_control(PIN_E_GPIO_21,QAPI_GPIO_INPUT_E); //ACC_INT2

	//set default value for gpio	
	gpio_value_control(PIN_E_GPIO_02, HIGH);
	gpio_value_control(PIN_E_GPIO_04, HIGH);
	gpio_value_control(PIN_E_GPIO_05, HIGH);
	gpio_value_control(PIN_E_GPIO_09, HIGH);
	gpio_value_control(PIN_E_GPIO_19, HIGH);
	gpio_value_control(PIN_E_GPIO_20, HIGH);
	//gpio_value_control(PIN_E_GPIO_21, TRUE);

	// output debug info to main uart1
	atel_dbg_print("gpio config to default value!\n");
	
}





/*
  func: build report msg
  -------------------------
  -------------------------
  output: point to the report msg
*/
void msg_rep_build(msg_rep_u *msg)
{
	/* get response from AT+QGPSLOC? */

	/* fill each mem of the specific data format */

	
	return;
}


void auto_rep_func(ULONG timer_input)
{
	
	return;
}

boolean system_timer_init(void)
{
	ULONG status = 0xff;
	status = tx_timer_create(&auto_report, "auto report", auto_rep_func, 0, EXPIRE_TICK, EXPIRE_TICK, TX_NO_ACTIVATE);
	if(TX_SUCCESS != status)
	{
		atel_dbg_print("[system_timer_init] tx_timer_create failed!");
		return FALSE;
	}

	
	return TRUE;
}

/*
@func
  data_capture_process
@brief
  data capture thread. 
*/

int data_capture_process(void)
{
	/* simulate data copy to temporary memory */
	while (1) 
	{
		tmp_mem[frame_index][0] = tx_time_get();
		tmp_mem[frame_index][1] = 0x1234;/* simulate the address the actual data */
		frame_index = (frame_index + 1) % MAX_TMP_MEM;
		/* sleep one tick */
		tx_thread_sleep(1);
	}
	
	return 0;
}


/*
@func
  event_det
@brief
  events process entry. 
*/

int event_record(void)
{
	#if 0
	ULONG event_flag = 0;
	char *event_type = NULL;
	/* check event bitmap */
	tx_event_flags_get(events_signal_handle, EVENTS_SIG_ALL, TX_OR_CLEAR, &event_flag, TX_WAIT_FOREVER);
	
	/* receive sig for auto report event */
	if(event_flag&AUTO_REPORT_EVT)
	{
		/* obtain message type */
		event_type = get_event_type(AUTO_REPORT_EVT);

		/* send msg to data process thread */
		//tx_queue_send(evt_queue, );
		/* active a timer to capture tempory auto report data */
		tx_timer_activate(&auto_report);
	}
	else if(event_flag&IG_ON_EVT)
	{
		atel_dbg_print("receive igniton on msg");
		
	}
	else
	{
		atel_dbg_print("under development");
	}
	#endif

	ULONG frame, event_priority, event_time, index, frame_data[2];
	while (1)
	{
		/* Copy an event from temporary memory to report buffer. */
		/* Get frame_index from event message queue and copy 24 frames */
		/* from temp_memory to report buffer. */
		tx_queue_receive(&evt_notice, frame_data, TX_NO_WAIT);
		/* Store event time and event priority in protected memory */
		frame = frame_data[0];
		event_priority = frame_data[1];
		
		event_time = tmp_mem[frame][0];
		
		atel_dbg_print("**Event** Time: %5lu Count: %2lu Pri: %lu", event_time, event_count, event_priority);
		
		if (event_count < MAX_EVENTS)
		{
			/* make sure only one event get the lock */
			tx_mutex_get(&rep_mem_mutex, TX_WAIT_FOREVER);
			
			rep_buf[event_count][0] = event_time;
			rep_buf[event_count][1] = event_priority;
			
			if (frame < 11)
				frame = (MAX_TMP_MEM-1) - (frame_index+1);
			
			for (index = 0; index < 24; index++)
			{
				rep_buf[event_count][index+2] = tmp_mem[frame][1];
				frame = (frame+1) % MAX_TMP_MEM;
			}
			/* send head msg to server */
			//qapi_sendto(auto_rep_fd, );
			
			tx_mutex_put(&rep_mem_mutex);
			event_count++;
		}
		else 
			atel_dbg_print(" **not processed**");
		
		tx_thread_suspend(&evt_rep_config.module_thread_handle);
	}

	return 0;
	
}

/* begin: timer function */
void unsafe_ISR(ULONG timer_input)
{
	ULONG frame_data[2];
	frame_data[0] = frame_index;
	frame_data[1] = MIDDLE_E;/* prio of the event */
	num_unsafe++;
	/* Activates timer to expire in 12 seconds - end of event */
	/* Put frame_index and priority on queue for crash events */
	tx_queue_send(&evt_notice, frame_data, TX_NO_WAIT);
	tx_timer_activate(&unsafe_copy_scheduler);
	
	return;
}

void warning_ISR(ULONG timer_input)
{	
	ULONG frame_data[2];
	frame_data[0] = frame_index;
	frame_data[1] = NORMAL_E;/* prio of the event */
	num_warning++;
	/* Activates timer to expire in 12 seconds - end of event */
	/* Put frame_index and priority on queue for crash events */
	tx_queue_send(&evt_notice, frame_data, TX_NO_WAIT);
	tx_timer_activate(&warning_copy_scheduler);
	return;
}

void unsafe_copy_activate(ULONG timer_input)
{
	/* resume recorder thread to initiate data recording */
	tx_thread_resume(&evt_rep_config.module_thread_handle);
	tx_timer_deactivate(&unsafe_copy_scheduler);
	
	return;
}

void warning_copy_activate(ULONG timer_input)
{
	/* resume recorder thread to initiate data recording */
	tx_thread_resume(&evt_rep_config.module_thread_handle);
	tx_timer_deactivate(&warning_copy_scheduler);
	return;
}

void print_stats (ULONG invalue)
{
	UINT row, col;
	atel_dbg_print("\n SLP01 System Periodic Event Summary");
	atel_dbg_print(" Current Time: %lu\n", tx_time_get());
	atel_dbg_print(" Number of Unsafe Events: %lu\n", num_unsafe);
	atel_dbg_print(" Number of Warnings: %lu\n", num_warning);
	
	if (event_count > 0)
	{
		atel_dbg_print("\n\n**** Portion of Protected Memory Contents\n\n");
		atel_dbg_print("%6s%6s%6s\n", "Time", "Pri", "Data");
		for (row = 0; row < event_count; row++)
		{
			for (col = 0; col < 8; col++)
				atel_dbg_print("%6lu", rep_buf[row][col]);
			atel_dbg_print(" (etc.)\n");
		}
	}
	
	if (event_count >= MAX_EVENTS)
		atel_dbg_print(" Warning: report buffer is full...");

	return;
}




/* end */

/*
@func
  event_early_parse
@brief
  creat msg queue to communicate with udp thread. 
*/

void event_early_init(void)
{
	
	ULONG status = 0xff;
	ULONG msg_size = 0;
	
	/* create event signal for detecting events */
    txm_module_object_allocate(&events_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(events_signal_handle, "events signal");
	tx_event_flags_set(events_signal_handle, 0x0, TX_AND);

	/* create message queue for further process data report */
	status = txm_module_object_allocate(&byte_pool_event, sizeof(TX_BYTE_POOL));
  	if(status != TX_SUCCESS)
  	{
  		atel_dbg_print("[main thread] txm_module_object_allocate [byte_pool_event] failed, %d", status);
  	}
	status = tx_byte_pool_create(byte_pool_event, "event report pool", event_pool_start, FREE_MEM_SIZE);
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[quectel_task_entry]byte_pool_event create failed!");
	}

	/* begin: init timer */
	
	status = txm_module_object_allocate(&unsafe_intr, sizeof(TX_TIMER));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate unsafe_intr failed, %d", status);
	}
	
	status = txm_module_object_allocate(&warning_intr, sizeof(TX_TIMER));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate warning_intr failed, %d", status);
	}

	status = txm_module_object_allocate(&unsafe_copy_scheduler, sizeof(TX_TIMER));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate unsafe_intr failed, %d", status);
	}
	
	status = txm_module_object_allocate(&warning_copy_scheduler, sizeof(TX_TIMER));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate warning_intr failed, %d", status);
	}

	status = txm_module_object_allocate(&stats_timer, sizeof(TX_TIMER));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate warning_intr failed, %d", status);
	}
	
	/* timer to simulate event and alarm */
	tx_timer_create(&unsafe_intr, "unsafe_interrupt", unsafe_ISR, 0x1234, 760, 760, TX_AUTO_ACTIVATE);
	tx_timer_create(&warning_intr, "warning_interrupt", warning_ISR, 0x1234, 410, 410, TX_AUTO_ACTIVATE);
	/* timer for send msg to data process thread */
	tx_timer_create(&unsafe_copy_scheduler, "unsafe_copy_scheduler", unsafe_copy_activate, 0x1234, 12, 12, TX_NO_ACTIVATE);
	tx_timer_create(&warning_copy_scheduler, "warning_copy_scheduler", warning_copy_activate, 0x1234, 12, 12, TX_NO_ACTIVATE);
	/* Create and activate the timer to print statistics periodically */
	tx_timer_create (&stats_timer, "stats_timer", print_stats, 0x1234, 1000, 1000, TX_AUTO_ACTIVATE);

	/* end */
	
	status = txm_module_object_allocate(&evt_notice, sizeof(TX_QUEUE));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate evt_notice failed, %d", status);
	}

	/* critcal region protect while send msg to server */
	status = txm_module_object_allocate(&rep_mem_mutex, sizeof(TX_MUTEX));
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[main thread] txm_module_object_allocate rep_mem_mutex failed, %d", status);
	}
	
	status = tx_byte_allocate(byte_pool_event, (void **)&event_que_handle, QUEUE_NODE_MAX * sizeof(event_msg), TX_NO_WAIT);
	if(status != TX_SUCCESS)
	{
		atel_dbg_print("[quectel_task_entry] tx_byte_allocate event_msg failed");
	}
	/* to be 32-bit words long */
	msg_size = sizeof(event_msg)/sizeof(uint32);
	status = tx_queue_create(evt_notice, "event transmit", msg_size, event_que_handle, QUEUE_NODE_MAX * sizeof(event_msg));
	if (TX_SUCCESS != status)
	{
		atel_dbg_print("[quectel_task_entry]tx_queue_create failed with status %d", status);
	}

	return;

}



/*
@func
  quectel_task_entry
@brief
  Entry function for task. 
*/
/*=========================================================================*/
int quectel_task_entry(void)
{
	AE_BITMAP_T bitmap = {0};
	boolean cur_ig_status = FALSE;
	IG_ON_RUNNING_MODE_E ig_on_mode = IG_ON_WATCH_M;
	ULONG sig_mask = IG_SWITCH_OFF_E | IG_SWITCH_ON_E;
	ULONG switch_event = 0;
	char *sock_buf = NULL;
	UINT status = 0xff;
	int msg_size;
	/* wait 10sec for device startup */
	qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);


	/* slp01 main task start */
	atel_dbg_print("[slp01 main task] start up ...\n");
	#if 1
	/* OTA service start */
	ota_service_start();

	/* init gpio */
	gpio_init();

	/* init open source for event report */
	//event_early_init();
	
#ifndef ATEL_DEBUG
	/* ignition timer init */
	ig_timer_init();
#endif

#ifndef IF_SERVER_DEBUG
	atel_dbg_print("cmd_parse_start......");
	cmd_parse_entry(sock_buf);
	atel_dbg_print("[ok]");
#endif

#ifdef ATEL_BG96_COM_BLE
	/* register AT command framework to support communicate with BLE */
	if (TX_SUCCESS != bg96_com_ble_task())
	{
		atel_dbg_print("[atfwd] framwork register failed!");
	}
#endif 

	/* system init */
	if(TRUE != system_init())
	{
		atel_dbg_print("sys init failed!");
	}


	normal_start = TRUE;
	
	/* create signal handler for detecting ig stauts  */
    txm_module_object_allocate(&ig_switch_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(ig_switch_signal_handle, "ignition status tracking");
	tx_event_flags_set(ig_switch_signal_handle, 0x0, TX_AND);

	#if 0
	/* enter ig off flow when system normal up */
	if (normal_start)
	{
		atel_dbg_print("[ignition] enter init ig off process!");
		ig_off_process();
	}
	#endif

#ifdef ATEL_DEBUG
	//daily_report_start();
	/* init queue for gps data */
	rq_init();
	/* Init timer for sending data every 5seconds */
	udp_data_transfer_timer_def();
	udp_data_transfer_timer_start();
#endif
	
	atel_dbg_print("Entering mode switch flow");
	
	while(1)
	{
		atel_dbg_print("[quectel_task_entry]main task running %d times", main_task_run_cnt++);

		#if 0
		/* get switch event */
		tx_event_flags_get(ig_switch_signal_handle, sig_mask, TX_OR, &switch_event, TX_WAIT_FOREVER);
		
		atel_dbg_print("switch_event: %d,IG_S_ON: %d, IG_S_OFF: %d\n", switch_event, IG_SWITCH_ON_E, IG_SWITCH_OFF_E);
		
		/* ignition off */
		if(switch_event&IG_SWITCH_OFF_E)
		{
			#ifdef ATEL_DEBUG
			atel_dbg_print("switch to ig off flow");
			#endif
			//BIT_SET(bitmap.ig_off_e, IG_OFF_E); /* denote ig off event */
			tx_event_flags_set(ig_switch_signal_handle, ~IG_SWITCH_OFF_E, TX_AND);
			ig_off_process();
		}
		else if(switch_event&IG_SWITCH_ON_E)
		{
			#ifdef ATEL_DEBUG
			atel_dbg_print("switch to ig on flow");
			#endif
			//BIT_SET(bitmap.ig_on_e, IG_ON_E); /* denote ig on event */
			tx_event_flags_set(ig_switch_signal_handle, ~IG_SWITCH_ON_E, TX_AND);
			/* mode detect */
			ig_on_mode = check_running_mode(ATEL_IG_ON_RUNNING_MODE_CFG_FILE);
			ig_on_process(ig_on_mode);
		}

		/* clear all events */
		//tx_event_flags_set(&ig_switch_signal_handle, 0x0, TX_AND);
		#endif
#ifdef ATEL_DEBUG
		qapi_Timer_Sleep(2, QAPI_TIMER_UNIT_SEC, true);
#endif

	}
	#endif 
	atel_dbg_print("[slp01 main task] Exit\n");
}

#endif /*__EXAMPLE_BG96_APP__*/


