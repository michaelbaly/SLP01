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





/**************************************************************************
*                                 DEFINE
***************************************************************************/
#define ATEL_DEBUG

qapi_TIMER_handle_t timer_handle;
qapi_TIMER_define_attr_t timer_def_attr;
qapi_TIMER_set_attr_t timer_set_attr;

#define ATEL_GPS_LOCATION_DIR_PATH		"/datatx/atel_gps"
#define ATEL_GPS_LOCATION_LOG_FILE		"/datatx/atel_gps/gps.log"
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

#define LAT_LONG_UPDATE_FREQ 1000 /* UNIT:ms */


static TX_EVENT_FLAGS_GROUP gps_signal_handle;
static TX_EVENT_FLAGS_GROUP gps_file_handle;
static uint32 gps_tracking_id;


static int timer_count = 0;



char tcp_send_buffer[256] = {0};
static char loc_trans_buffer[256] = {0};
int  g_atel_sockfd = -1;
/* Modify this pin num which you want to test */
MODULE_PIN_ENUM  g_gpio_pin_num = PIN_E_GPIO_20;


subtask_config_t tcpclient_task_config ={
    
    NULL, "Atel Tcpclient Task Thread", atel_tcpclient_start, \
    atel_tcpclient_thread_stack, ATEL_TCPCLIENT_THREAD_STACK_SIZE, ATEL_TCPCLIENT_THREAD_PRIORITY
    
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
    	/* write gps location to log file */
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
	timer_set_attr.time = 60;
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

void system_init(void)
{
	/* tcp service init */

	/* gps init */

	
	return;
}

boolean vehicle_motion_detect(void)
{
	/*  */
	return;
}

void gps_loc_timer_start()
{
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
	boolean ig_status = FALSE;
	/* OTA service start */
	ota_service_start();

	/* system init */
	system_init();

	/* vehicle motion detect */
	ig_status = vehicle_motion_detect();
	
	while(ig_status)
	{
		/* report init location */

		/* update gps loc via timer */
		gps_loc_timer_init();
	}
}

#endif /*__EXAMPLE_BG96_APP__*/


