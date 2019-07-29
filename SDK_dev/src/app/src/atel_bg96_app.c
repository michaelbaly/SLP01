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





/**************************************************************************
*                                 DEFINE
***************************************************************************/
#define ATEL_DEBUG

qapi_TIMER_handle_t timer_handle;
qapi_TIMER_define_attr_t timer_def_attr;
qapi_TIMER_set_attr_t timer_set_attr;

#define ATEL_IG_ON_RUNNING_MODE_CFG_FILE		"/datatx/ig_mode_cfg"
/**************************************************************************
*                                 GLOBAL
***************************************************************************/

TX_TIMER gpio_sim = {0};


#define LOC_SAVE_OK		0
#define LOC_SAVE_ERR	-1


/* begin: MSO feature */
#define MSO_COUNTER_PRESET	5
static char g_mso_counter = 0;
/* end */

/* begin: sim relay from ble */
boolean g_sim_relay_ble = LOW;
boolean g_timer_switch = FALSE;
static boolean g_sim_counter = 0;
/* end */


//static TX_EVENT_FLAGS_GROUP gps_file_handle;

/* begin: for ignition status switch signal */
TX_EVENT_FLAGS_GROUP *ig_switch_signal_handle;

/* end */



static boolean normal_start = 0;

/* Modify this pin num which you want to test */
MODULE_PIN_ENUM  g_gpio_pin_num = PIN_E_GPIO_20;


subtask_config_t tcpclient_task_config ={
    
    NULL, "Atel Tcpclient Task Thread", atel_tcpclient_entry, \
    atel_tcpclient_thread_stack, ATEL_TCPCLIENT_THREAD_STACK_SIZE, ATEL_TCPCLIENT_THREAD_PRIORITY
    
};
	
subtask_config_t gps_task_config ={
	
	NULL, "Atel GPS Task Thread", atel_gps_entry, \
	atel_gps_thread_stack, ATEL_GPS_THREAD_STACK_SIZE, ATEL_GPS_THREAD_PRIORITY
	
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
	qapi_Timer_Sleep(50, QAPI_TIMER_UNIT_MSEC, true);

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

int gps_service_start(void)
{
	int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&gps_task_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		atel_dbg_print("[task_create] txm_module_object_allocate failed ~");
		return - 1;
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
	g_sim_counter++;
	atel_dbg_print("[timer callback]g_sim_counter is: %d", g_sim_counter);
	if(g_sim_counter % 2)
	{
		g_sim_relay_ble = HIGH;
	}
	else 
	{
		g_sim_relay_ble = LOW;
	}
	
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
	atel_dbg_print("[TIMER] status[%d]", status);

	memset(&timer_set_attr, 0, sizeof(timer_set_attr));
	timer_set_attr.reload = 10;
	timer_set_attr.time = 10;
	timer_set_attr.unit = QAPI_TIMER_UNIT_SEC;
	status = qapi_Timer_Set(timer_handle, &timer_set_attr);
}

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



void system_init(void)
{

	/* tcp service bring up */
	tcp_service_start();
	
	/* gps service bring up */
    gps_service_start();
	
	return;
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
    g_mso_counter = MSO_COUNTER_PRESET;

	
	return;
}

void ig_off_process()
{
	
	char *ig_timer_name = NULL;
	UINT active = 0xff;
	ULONG remaining_ticks = 0;
	ULONG reschedule_ticks = 0;
	TX_TIMER * next_timer;
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
			atel_dbg_print("[igition]off ---> on!");
			tx_event_flags_set(ig_switch_signal_handle, IG_SWITCH_ON_E, TX_OR);
			
			/* terminate ig off data flow */
			break;
		}


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
			atel_dbg_print("[igition]on ---> off!");
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


void daily_heartbeat_rep(void)
{
	/* init the timer: reload timer */

    /* report gps location once timer expires */

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
	
	/* wait 5sec for device startup */
	//qapi_Timer_Sleep(10, QAPI_TIMER_UNIT_SEC, true);

	/* slp01 main task start */
	atel_dbg_print("[slp01 main task] start up ...\n");
	#if 1
	/* OTA service start */
	ota_service_start();

	/* init gpio */
	gpio_init();

	/* ignition timer init */
	ig_timer_init();


	#ifdef ATEL_BG96_COM_BLE
	/* register AT command framework to support communicate with BLE */
	if (0 == bg96_com_ble_task())
	{
		atel_dbg_print("[atfwd] framwork successfully registered!");
	}

	#endif 

	/* system init */
	//system_init();

	/* daily heart beat report after system up */
	daily_heartbeat_rep();

	normal_start = TRUE;
	
	/* create signal handler to receive  */
    txm_module_object_allocate(&ig_switch_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	tx_event_flags_create(ig_switch_signal_handle, "ignition status tracking");
	tx_event_flags_set(ig_switch_signal_handle, 0x0, TX_AND);

	
	/* enter ig off flow when system normal up */
	if (TRUE == normal_start)
	{
		atel_dbg_print("[ignition] enter init ig off process!");
		ig_off_process();
	}

	
	atel_dbg_print("Entering mode switch flow");
	
	while(1)
	{
		atel_dbg_print("switch flow running...");
		/* get switch event */
		tx_event_flags_get(ig_switch_signal_handle, sig_mask, TX_OR, &switch_event, TX_WAIT_FOREVER);
		
		atel_dbg_print("switch_event: %d,IG_S_OFF: %d, IG_S_ON: %d\n", switch_event, IG_SWITCH_OFF_E, IG_SWITCH_ON_E);
		
		/* ignition off */
		if(switch_event&IG_SWITCH_OFF_E)
		{
			#ifdef ATEL_DEBUG
			atel_dbg_print("switch to ig off flow");
			#endif
			BIT_SET(bitmap.ig_off_e, IG_OFF_E); /* denote ig off event */
			tx_event_flags_set(ig_switch_signal_handle, ~IG_SWITCH_OFF_E, TX_AND);
			ig_off_process();
		}
		else if(switch_event&IG_SWITCH_ON_E)
		{
			#ifdef ATEL_DEBUG
			atel_dbg_print("switch to ig on flow");
			#endif
			BIT_SET(bitmap.ig_on_e, IG_ON_E); /* denote ig on event */
			tx_event_flags_set(ig_switch_signal_handle, ~IG_SWITCH_ON_E, TX_AND);
			/* mode detect */
			ig_on_mode = check_running_mode(ATEL_IG_ON_RUNNING_MODE_CFG_FILE);
			ig_on_process(ig_on_mode);
		}

		/* clear all events */
		//tx_event_flags_set(&ig_switch_signal_handle, 0x0, TX_AND);

		qapi_Timer_Sleep(5, QAPI_TIMER_UNIT_SEC, true);

	}
	#endif 
	atel_dbg_print("[slp01 main task] Exit\n");
}

#endif /*__EXAMPLE_BG96_APP__*/


