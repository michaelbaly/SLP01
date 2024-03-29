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
#include "quectel_utils.h"
#include "quectel_uart_apis.h"
#include "atel_bg96_app.h"
//#include "events_if.h"





/**************************************************************************
*                                 DEFINE
***************************************************************************/
#define ATEL_DEBUG

qapi_TIMER_handle_t timer_handle;
qapi_TIMER_define_attr_t timer_def_attr;
qapi_TIMER_set_attr_t timer_set_attr;

#define ATEL_GPS_LOCATION_DIR_PATH				"/datatx/atel_gps"
#define ATEL_GPS_LOCATION_LOG_FILE				"/datatx/atel_gps/gps.log"
#define ATEL_IG_ON_RUNNING_MODE_CFG_FILE		"/datatx/ig_mode_cfg"
/**************************************************************************
*                                 GLOBAL
***************************************************************************/
#define LOCATION_GPS_LOG_FILE_BIT 		(1 << 0)
#define LOCATION_TRACKING_RSP_OK_BIT 	(1 << 1)
#define LOCATION_TRACKING_RSP_FAIL_BIT 	(1 << 2)

#define S_IFDIR        	0040000 	/**< Directory */
#define S_IFMT          0170000		/**< Mask of all values */
#define S_ISDIR(m)     	(((m) & S_IFMT) == S_IFDIR)

#define MAX_BIT_MASK		8

#define LOC_SAVE_OK		0
#define LOC_SAVE_ERR	-1

#define LAT_LONG_UPDATE_FREQ 15000 /* UNIT:ms */

/* begin: MSO feature */
#define MSO_COUNTER_PRESET	5
static char g_mso_counter = 0;

/* end */

/* begin: sim relay from ble */
boolean g_sim_relay_ble;
boolean g_sim_counter;
/* end */


static TX_EVENT_FLAGS_GROUP gps_signal_handle;
static TX_EVENT_FLAGS_GROUP gps_file_handle;
static uint32 gps_tracking_id;

/* begin: for ignition status switch signal */
static TX_EVENT_FLAGS_GROUP ig_switch_signal_handle;

/* end */


static int timer_count = 0;

static boolean normal_start = 0;

char tcp_send_buffer[256] = {0};
static char loc_trans_buffer[256] = {0};
int  g_atel_sockfd = -1;
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




/* GPS information which will get in the corresponding callback function */
qapi_Location_t location;
qapi_loc_client_id loc_clientid;

/* Provides the capabilities of the system */
static void location_capabilities_callback(qapi_Location_Capabilities_Mask_t capabilities)
{

    qt_uart_dbg("Location mod has tracking capability:%d\n",capabilities);
}

static void location_response_callback(qapi_Location_Error_t err, uint32_t id)
{
    qt_uart_dbg("LOCATION RESPONSE CALLBACK err=%u id=%u", err, id); 
	if(err == QAPI_LOCATION_ERROR_SUCCESS)
	{
		tx_event_flags_set(&gps_signal_handle, LOCATION_TRACKING_RSP_OK_BIT, TX_OR);	
	}
	else
	{
		tx_event_flags_set(&gps_signal_handle, LOCATION_TRACKING_RSP_FAIL_BIT, TX_OR);	
	}
}

static void location_geofence_response_callback(size_t count,
                                                qapi_Location_Error_t* err,
                                                uint32_t* ids)
{
    qt_uart_dbg("LOC API CALLBACK : GEOFENCE RESPONSE");
}

/* It is called when delivering a location in a tracking session */
static void location_tracking_callback(qapi_Location_t loc)
{
	int send_len = -1;
	/* store loc info in location var */
    if(loc.flags & QAPI_LOCATION_HAS_LAT_LONG_BIT) 
    {   
        
		
		/* transform the loc info */
		loc_info_transform(loc, tcp_send_buffer);
        
		/* send a signal every 1 min, base on the location update frequency(1sec) */
		if(timer_count++ == 60)
		{
			memcpy(loc_trans_buffer, tcp_send_buffer, strlen(tcp_send_buffer));
			tx_event_flags_set(&gps_signal_handle, LOCATION_GPS_LOG_FILE_BIT, TX_OR);
			qt_uart_dbg("GPS location log file event signal sent");

			timer_count = 0;
		}
		
		/* g_atel_sockfd is filled when client has connect the server */
		send_len = qapi_send(g_atel_sockfd, tcp_send_buffer, strlen(tcp_send_buffer), 0);
		if(send_len > 0)
		{
			qt_uart_dbg("tcp_send_buffer @send:\n%s, @len: %d\n", tcp_send_buffer, strlen(tcp_send_buffer));
			memset(tcp_send_buffer, 0, sizeof(tcp_send_buffer));
		}

    }    
}


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


qapi_Location_Callbacks_t location_callbacks= {
    sizeof(qapi_Location_Callbacks_t),
    location_capabilities_callback,
    location_response_callback,
    location_geofence_response_callback,
    location_tracking_callback, //optional
    NULL,
    NULL
};

void location_init(void)
{
    qapi_Location_Error_t ret;

    tx_event_flags_create(&gps_signal_handle, "ts_gps_app_events");

    ret = qapi_Loc_Init(&loc_clientid, &location_callbacks);
    if (ret == QAPI_LOCATION_ERROR_SUCCESS)
    {
        qt_uart_dbg("LOCATION_INIT SUCCESS");
    }
    else
    {
        qt_uart_dbg("LOCATION_INIT FAILED");
        return;
    }
}

void location_deinit(void)
{

    qapi_Loc_Stop_Tracking(loc_clientid, gps_tracking_id);
    qapi_Loc_Deinit(loc_clientid);
    tx_event_flags_delete(&gps_signal_handle);
}

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  loc_info_transform
@brief
  Show location info.
*/
void loc_info_transform(qapi_Location_t location, char* trans_buf)
{
    char shift_count = 0;
	unsigned char loc_bit;
	int write_len = 0;
	int total_len = 0;
	
	while(shift_count < MAX_BIT_MASK)
	{
		loc_bit = 1 << shift_count;
		if(location.flags & loc_bit) 
		{   
			switch(loc_bit)
			{
				case QAPI_LOCATION_HAS_LAT_LONG_BIT:
					write_len = sprintf(trans_buf, "LATITUDE: %d.%d\r\n",
						(int)location.latitude, (abs((int)(location.latitude * 100000))) % 100000);
					total_len += write_len; 
					write_len = sprintf(trans_buf + total_len, "LONGITUDE: %d.%d\r\n",
						(int)location.longitude, (abs((int)(location.longitude * 100000))) % 100000);
					total_len += write_len; 
					break;

				case QAPI_LOCATION_HAS_ALTITUDE_BIT:
					write_len = sprintf(trans_buf + total_len, "ALTITUDE: %d.%d\r\n",
						(int)location.altitude, (abs((int)(location.altitude * 100))) % 100);
					
					total_len += write_len; 
					break;

				case QAPI_LOCATION_HAS_SPEED_BIT:
					write_len = sprintf(trans_buf + total_len, "SPEED: %d.%d\r\n",
						(int)location.speed, (abs((int)(location.speed * 100))) % 100);
					
					total_len += write_len; 
					break;

				case QAPI_LOCATION_HAS_BEARING_BIT:
					write_len = sprintf(trans_buf + total_len, "BEARING: %d.%d\r\n",
						(int)location.bearing, (abs((int)(location.bearing * 100))) % 100);
					total_len += write_len; 
					break;
					
				case QAPI_LOCATION_HAS_ACCURACY_BIT:
					write_len = sprintf(trans_buf + total_len, "ACCURACY: %d.%d\r\n",
						(int)location.accuracy, (abs((int)(location.accuracy * 100))) % 100);
					total_len += write_len; 
					break;
					
				case QAPI_LOCATION_HAS_VERTICAL_ACCURACY_BIT:
					write_len = sprintf(trans_buf + total_len, "VERTICAL ACCURACY: %d.%d\r\n",
						(int)location.verticalAccuracy, (abs((int)(location.verticalAccuracy * 100))) % 100);
					total_len += write_len; 
					break;

				case QAPI_LOCATION_HAS_SPEED_ACCURACY_BIT:
					write_len = sprintf(trans_buf + total_len, "SPEED ACCURACY: %d.%d\r\n",
						(int)location.speedAccuracy, (abs((int)(location.speedAccuracy * 100))) % 100);
					total_len += write_len; 
					break;

				case QAPI_LOCATION_HAS_BEARING_ACCURACY_BIT:
					write_len = sprintf(trans_buf + total_len, "BEARING ACCURACY: %d.%d\r\n",
						(int)location.bearingAccuracy, (abs((int)(location.bearingAccuracy * 100))) % 100);
					total_len += write_len; 
					break;
					
			}
				
		}
		shift_count++;
	}
}


/*
@func
  location_info_show
@brief
  Show location info.
*/

void location_info_show()
{
	char shift_count = 0;
	unsigned char loc_bit;
	
    uint64_t ts_sec = location.timestamp / 1000;
	qt_uart_dbg("location timestamp: %lld", ts_sec);
    qt_uart_dbg("Time(HH:MM:SS): %02d:%02d:%02d", \
		        (int)((ts_sec / 3600) % 24), \
                (int)((ts_sec / 60) % 60), \
                (int)(ts_sec % 60));
	
	while(shift_count < MAX_BIT_MASK)
	{
		loc_bit = 1 << shift_count;
		if(location.flags & loc_bit) 
		{   
			switch(loc_bit)
			{
				case QAPI_LOCATION_HAS_LAT_LONG_BIT:
					qt_uart_dbg("LATITUDE: %d.%d",
						(int)location.latitude, (abs((int)(location.latitude * 100000))) % 100000);
					qt_uart_dbg("LONGITUDE: %d.%d",
						(int)location.longitude, (abs((int)(location.longitude * 100000))) % 100000);
					break;

				case QAPI_LOCATION_HAS_ALTITUDE_BIT:
					qt_uart_dbg("ALTITUDE: %d.%d",
						(int)location.altitude, (abs((int)(location.altitude * 100))) % 100);
					break;

				case QAPI_LOCATION_HAS_SPEED_BIT:
					qt_uart_dbg("SPEED: %d.%d",
						(int)location.speed, (abs((int)(location.speed * 100))) % 100);
					break;

				case QAPI_LOCATION_HAS_BEARING_BIT:
					qt_uart_dbg("BEARING: %d.%d",
						(int)location.bearing, (abs((int)(location.bearing * 100))) % 100);
					break;
					
				case QAPI_LOCATION_HAS_ACCURACY_BIT:
					qt_uart_dbg("ACCURACY: %d.%d",
						(int)location.accuracy, (abs((int)(location.accuracy * 100))) % 100);
					break;
					
				case QAPI_LOCATION_HAS_VERTICAL_ACCURACY_BIT:
					qt_uart_dbg("VERTICAL ACCURACY: %d.%d",
						(int)location.verticalAccuracy, (abs((int)(location.verticalAccuracy * 100))) % 100);
					break;

				case QAPI_LOCATION_HAS_SPEED_ACCURACY_BIT:
					qt_uart_dbg("SPEED ACCURACY: %d.%d",
						(int)location.speedAccuracy, (abs((int)(location.speedAccuracy * 100))) % 100);
					break;

				case QAPI_LOCATION_HAS_BEARING_ACCURACY_BIT:
					qt_uart_dbg("BEARING ACCURACY: %d.%d",
						(int)location.bearingAccuracy, (abs((int)(location.bearingAccuracy * 100))) % 100);
					break;
					
			}
				
		}
		shift_count++;
	}
}

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

int gps_loc_log_write(void)
{
	int fd = -1;
	//char   file_buff[1024] ;
	int   wr_bytes = 0;
	qapi_FS_Offset_t seek_offset = 0;
    qapi_FS_Status_t status = QAPI_OK;
	
	/* write loc info to the file we specify */
	status = qapi_FS_Open(ATEL_GPS_LOCATION_LOG_FILE, QAPI_FS_O_RDWR_E, &fd);
	if((QAPI_OK == status) && (-1 != fd))
	{
		status = qapi_FS_Seek(fd, 0, QAPI_FS_SEEK_END_E, &seek_offset);
#ifdef ATEL_DEBUG
		qt_uart_dbg("qapi_FS_Seek: status ---> %d, seek_offset --->%lld", status, seek_offset);
		/* the content of location */
		qt_uart_dbg("gps_loc_log_write: content ---> %s", loc_trans_buffer);
#endif

        /* ensure that the current location data is not flushed by callback */
		status = qapi_FS_Write(fd, (uint8*)loc_trans_buffer, strlen(loc_trans_buffer), &wr_bytes);
#ifdef ATEL_DEBUG
		qt_uart_dbg("qapi_FS_Write: status --->%d, wr_bytes --->%d", status, wr_bytes);
#endif
		qapi_FS_Close(fd);
		return 0;
	}
	else
    {
#ifdef ATEL_DEBUG
        qt_uart_dbg("qapi_FS_Open: status --->%d, fd --->%d", status, fd);
#endif
    }
	qapi_FS_Close(fd);
	return -1;
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
	qt_uart_dbg("[TIMER] status[%d]", status);
#endif

	memset(&timer_set_attr, 0, sizeof(timer_set_attr));
	timer_set_attr.reload = true; /* timer will reactive once expires */
	timer_set_attr.time = 60; /* frequency we update the gps location */
	timer_set_attr.unit = QAPI_TIMER_UNIT_SEC;
	status = qapi_Timer_Set(timer_handle, &timer_set_attr);

#ifdef ATEL_DEBUG
	qt_uart_dbg("[TIMER] status[%d]", status);
#endif
}

/*
@func
  atel_efs_is_dir_exists
@brief
  find the path is a directory or directory is exist 
*/
int atel_efs_is_dir_exists(const char *path)
{
     int result, ret_val = 0;
     struct qapi_FS_Stat_Type_s sbuf;

     memset (&sbuf, 0, sizeof (sbuf));

     result = qapi_FS_Stat(path, &sbuf);

     if (result != 0)
          goto End;

     if (S_ISDIR (sbuf.st_Mode) == 1)
          ret_val = 1;

     End:
          return ret_val;
}


void gps_loc_log_init()
{

  	int    fd = -1;
	qapi_FS_Status_t status = QAPI_OK;
	/// MUST SETTING
	setlocale(LC_ALL, "C"); /// <locale.h>

	/* create dir if not exist */
	if(atel_efs_is_dir_exists(ATEL_GPS_LOCATION_DIR_PATH) != 1)
	{
		status = qapi_FS_Mk_Dir(ATEL_GPS_LOCATION_DIR_PATH, 0677);
		#ifdef ATEL_DEBUG
		qt_uart_dbg("qapi_FS_Mk_Dir %d", status);
		#endif
	}

	/* create log file if not exist */
	status = qapi_FS_Open(ATEL_GPS_LOCATION_LOG_FILE, QAPI_FS_O_CREAT_E | QAPI_FS_O_EXCL_E, &fd);
	if((QAPI_OK == status) && (-1 != fd))
	{
	    qapi_FS_Close(fd);
	}
	else
    {
		#ifdef ATEL_DEBUG
        qt_uart_dbg("qapi_FS_Open: status --->%d, fd --->%d", status, fd);
		#endif
    }

}

int ota_service_start(void)
{
	return;
}


char tcp_service_start(void)
{
    int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&tcpclient_task_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		qt_uart_dbg("[task_create] txm_module_object_allocate failed ~");
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
		qt_uart_dbg("[task_create] : Thread creation failed");
	}

	return;
}

void gps_service_start(void)
{
	int32 status = -1;
	
	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&gps_task_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		qt_uart_dbg("[task_create] txm_module_object_allocate failed ~");
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
		qt_uart_dbg("[task_create] : Thread creation failed");
	}
}

int bg96_com_ble_task(void)
{
	
	int32 status = -1;

	if(TX_SUCCESS != txm_module_object_allocate((VOID *)&mdm_ble_at_config.module_thread_handle, sizeof(TX_THREAD))) 
	{
		qt_uart_dbg("[task_create] txm_module_object_allocate failed ~");
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
		qt_uart_dbg("[task_create] : Thread creation failed");
		return -1;
	}

	return 0;
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
	ig_status = relay_from_ble();

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
	status = qapi_FS_Read(fd, (uint8*)file_buff, sizeof(file_buff), &rd_bytes)
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
		return IG_ON_DEFAULT_M
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
	#if 0
	/* whether enable MSO feature or not */
	if(motion_status && cell_coverage && gps_fix_valid)
	{
		/* report gps location */
		loc_report(FALSE);

		/* tell ble to disable relay */
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
		
		/* send signal to main task when ignition status switch to ignition on */
		if (HIGH == relay_from_ble())
		{
			tx_event_flags_set(&ig_switch_signal_handle, IG_SWITCH_ON_E, TX_OR);
			/* terminate ig off data flow */
			break;
		}
	}
	
	return ;
}

void drv_behav_detect(void)
{
	return;
}

void gpio_driver_random(ULONG gpio_phy)
{   
	g_sim_counter++;
	if(g_sim_counter % 2 )
	{
		g_sim_relay_ble = HIGH;
	}
	else 
	{
		g_sim_relay_ble = LOW;
	}
	
	return;
}

/*
@func
  relay_from_ble
@brief
  get relay status from ble. 
*/
/*=========================================================================*/
void relay_from_ble(void)
{
	TX_TIMER gpio_sim;
	ULONG init_expire_tick = 10000;
	/* use gpio to simulate relay value */
	tx_timer_create(&gpio_sim, "gpio sim timer", gpio_driver_random, 0, \
					init_expire_tick, 30000, TX_AUTO_ACTIVATE);
	
	return g_sim_relay_ble;
}

void ig_on_process(IG_ON_RUNNING_MODE_E mode)
{

	/* loc report and update base on the running mode */
	//loc_report(boolean update_flag);

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
			tx_event_flags_set(&ig_switch_signal_handle, IG_SWITCH_OFF_E, TX_OR);
			break;
		}
		
	}

	return;
}

void alarm_event_process(void)
{
	/* check alarm first */

    /* check event */

	return;
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

/* begin: uart depend functions */
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
	qapi_UART_Receive(uart_conf->hdlr, uart_conf->rx_buff, uart_conf->rx_len, (void*)uart_conf);
}

/* end */


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
	int sig_mask = IG_SWITCH_OFF_E | IG_SWITCH_ON_E;
	ULONG switch_event = 0;

	
	/* OTA service start */
	ota_service_start();

	/* init gpio */
	gpio_init();

	/* register AT command framework to support communicate with BLE */
	bg96_com_ble_task();

	/* system init */
	system_init();

	/* daily heart beat report after system up */
	daily_heartbeat_rep();

	normal_start = TRUE;
	/* create signal handler to receive  */
	tx_event_flags_create(&ig_switch_signal_handle, "ignition status tracking");
	tx_event_flags_set(&ig_switch_signal_handle, 0x0, TX_AND);

	
	/* enter ig off flow when system normal up */
	if (TRUE == normal_start)
	{
		ig_off_process();
	}
	
	while(1)
	{
		
		/* get switch event */
		tx_event_flags_get(&ig_switch_signal_handle, sig_mask, TX_OR, &switch_event, TX_NO_WAIT);
		
		/* ignition off */
		if(switch_event&IG_SWITCH_OFF_E)
		{
			#ifdef ATEL_DEBUG
			atel_dbg_print("switch to ig off flow");
			#endif
			BIT_SET(bitmap.alarm_eve, IG_OFF_E); /* denote ig off event */
			tx_event_flags_set(&ig_switch_signal_handle, ~IG_SWITCH_OFF_E, TX_AND);
			ig_off_process();
		}
		else if(switch_event&IG_SWITCH_ON_E)
		{
			#ifdef ATEL_DEBUG
			atel_dbg_print("switch to ig on flow");
			#endif
			BIT_SET(bitmap.alarm_eve, IG_ON_E); /* denote ig on event */
			tx_event_flags_set(&ig_switch_signal_handle, ~IG_SWITCH_ON_E, TX_AND);
			/* mode detect */
			ig_on_mode = check_running_mode(ATEL_IG_ON_RUNNING_MODE_CFG_FILE);
			ig_on_process(ig_on_mode);
		}

	}
}

#endif /*__EXAMPLE_BG96_APP__*/


