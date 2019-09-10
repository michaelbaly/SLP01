/******************************************************************************
*@file    example_gps.c
*@brief   example of gps operation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#if defined(__ATEL_BG96_APP__)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stdarg.h"
#include "qapi_fs_types.h"
#include "qapi_uart.h"
#include "qapi_timer.h"
#include "qapi_diag.h"

#include "quectel_utils.h"
#include "qapi_location.h"
#include "txm_module.h"
#include "quectel_uart_apis.h"
#include "atel_gps.h"


/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/*==========================================================================
  Signals used for waiting in test app for callbacks
===========================================================================*/
static TX_EVENT_FLAGS_GROUP* gps_signal_handle; 
static uint32 gps_tracking_id;

#define LOCATION_TRACKING_FIXED_BIT (0x1 << 0)
#define LOCATION_TRACKING_RSP_OK_BIT (0x1 << 1)
#define LOCATION_TRACKING_RSP_FAIL_BIT (0x1 << 2)
/* gps daily report source */
qapi_Location_t location;

#define MAX_BIT_MASK		8

#define GPS_USER_BUFFER_SIZE 4096
static uint8_t GPSuserBuffer[GPS_USER_BUFFER_SIZE];
qapi_loc_client_id loc_clnt;

char *udp_msg_addr = NULL;
/* begin: rq for cmd from server */
r_queue_s rq;
/* end */

extern TX_BYTE_POOL *byte_pool_event;
/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/* init the ring queue */
void rq_init()
{
	char cnt = 0;
	
	rq.front = rq.rear = 0;
	while(cnt < R_QUEUE_SIZE)
		rq.p_ack[cnt++] = NULL;
}

/* judge empty */
bool isempty(r_queue_s rq)
{
	if(rq.front == rq.rear)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/* judge full */
bool isfull(r_queue_s rq)
{
	if ((rq.rear + 1) % R_QUEUE_SIZE == rq.front)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/* enqueue */
void enqueue(char *cmd)
{
	if(isfull(rq))
	{
		atel_dbg_print("queue is full\n");
		return;
	}
	atel_dbg_print("[enqueue]the enqueue addr is %p\n", cmd);
	rq.p_ack[rq.rear] = cmd;
	rq.rear = (rq.rear + 1) % R_QUEUE_SIZE;

}

void dequeue()
{
	if (isempty(rq))
	{
		atel_dbg_print("queue is empty\n");
		return;
	}

	atel_dbg_print("[dequeue]the release addr is %p", rq.p_ack[rq.front]);
	/* release the allocated buff */
	tx_byte_release(rq.p_ack[rq.front]);

	/* move head pointer to next */
	rq.front = (rq.front + 1) % R_QUEUE_SIZE;

}

char * get_first_ele()
{
	if (isempty(rq))
	{
		atel_dbg_print("queue is empty\n");
		return NULL;
	}

	atel_dbg_print("[get_first_ele]the head addr is %p", rq.p_ack[rq.front]);
	return rq.p_ack[rq.front];
}


void show_queue_cont()
{
	char *tmp = rq.p_ack[rq.front];
	
	while(tmp)
	{
		atel_dbg_print("[cur_queue]%s", tmp);
		rq.front++;
		tmp = rq.p_ack[rq.front];
	}
}

/*==========================================================================
LOCATION API REGISTERED CALLBACKS
===========================================================================*/
static void location_capabilities_callback(qapi_Location_Capabilities_Mask_t capabilities)
{

    atel_dbg_print("Location mod has tracking capability:%d\n",capabilities);
}

static void location_response_callback(qapi_Location_Error_t err, uint32_t id)
{
    atel_dbg_print("LOCATION RESPONSE CALLBACK err=%u id=%u", err, id); 
	if(err == QAPI_LOCATION_ERROR_SUCCESS)
	{
		tx_event_flags_set(gps_signal_handle, LOCATION_TRACKING_RSP_OK_BIT, TX_OR);	
	}
	else
	{
		tx_event_flags_set(gps_signal_handle, LOCATION_TRACKING_RSP_FAIL_BIT, TX_OR);	
	}
}
static void location_collective_response_cb(size_t count, qapi_Location_Error_t* err, uint32_t* ids)
{
    
}

/* begin: transform location info */
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


/* end */

/* triggered per second generally */
static void location_tracking_callback(qapi_Location_t loc)
{
	int status = -1;
    if(loc.flags & QAPI_LOCATION_HAS_LAT_LONG_BIT) 
    {
    	/* allocate buf to store gps data */
	
		status = tx_byte_allocate(byte_pool_event, (void **)&udp_msg_addr, loc.size + 1, TX_NO_WAIT);
		if(status != TX_SUCCESS)
		{
			atel_dbg_print("[quectel_task_entry] tx_byte_allocate udp_msg_addr failed");
		}
		/* transform the loc info */
		//loc_info_transform(loc, udp_msg_addr);
		
		atel_dbg_print("allocate address is [%p]", udp_msg_addr);
		/* copy data */
		memset(udp_msg_addr, 0, loc.size + 1);
		strncpy(udp_msg_addr, &loc, loc.size);

		/* enqueue the gsp data */
		enqueue(udp_msg_addr);
        
    }    
}

qapi_Location_Callbacks_t location_callbacks= {
    sizeof(qapi_Location_Callbacks_t),
    location_capabilities_callback,
    location_response_callback,
    location_collective_response_cb,
    location_tracking_callback,
    NULL,
    NULL,
    NULL
};

/*==========================================================================
LOCATION INIT / DEINIT APIs
===========================================================================*/
void location_init(void)
{
    qapi_Location_Error_t ret;

    tx_event_flags_create(gps_signal_handle, "ts_gps_app_events");

    ret = qapi_Loc_Init(&loc_clnt, &location_callbacks);
    if (ret == QAPI_LOCATION_ERROR_SUCCESS)
    {
        atel_dbg_print("LOCATION_INIT SUCCESS");
    }
    else
    {
        atel_dbg_print("LOCATION_INIT FAILED");
    }
    ret = (qapi_Location_Error_t)qapi_Loc_Set_User_Buffer(loc_clnt, (uint8_t*)GPSuserBuffer, GPS_USER_BUFFER_SIZE);
    if (ret != QAPI_LOCATION_ERROR_SUCCESS) {
        atel_dbg_print("Set user buffer failed ! (ret %d)", ret);
    }
}

void location_deinit(void)
{

    qapi_Loc_Stop_Tracking(loc_clnt, gps_tracking_id);
    qapi_Loc_Deinit(loc_clnt);
    tx_event_flags_delete(gps_signal_handle);
}

int atel_gps_entry(void)
{
    uint32 signal = 0;
    uint32 count = 0;
    qapi_Location_Options_t Location_Options = {sizeof(qapi_Location_Options_t),
                                                1000,//update lat/lon frequency
                                                0};
    txm_module_object_allocate(&gps_signal_handle, sizeof(TX_EVENT_FLAGS_GROUP));
	qapi_Timer_Sleep(1000, QAPI_TIMER_UNIT_MSEC, true);//need delay 1s, otherwise maybe there is no callback.
    atel_dbg_print("[atel_gps_entry] thread start ~~~");  
	
    location_init();
    qapi_Loc_Start_Tracking(loc_clnt, &Location_Options, &gps_tracking_id);
	tx_event_flags_get(gps_signal_handle, LOCATION_TRACKING_RSP_OK_BIT|LOCATION_TRACKING_RSP_FAIL_BIT, TX_OR_CLEAR, &signal, TX_WAIT_FOREVER);
	if(signal&LOCATION_TRACKING_RSP_OK_BIT)
	{
		atel_dbg_print("[atel_gps_entry]wating for tracking...");
	}
	else if(signal&LOCATION_TRACKING_RSP_FAIL_BIT)
	{
		atel_dbg_print("start tracking failed");
		return -1;
	}
	while(1)
	{
	
		atel_dbg_print("[atel_gps_entry]---in gps main flow---");
        tx_event_flags_get(gps_signal_handle, LOCATION_TRACKING_FIXED_BIT, TX_OR_CLEAR, &signal, TX_WAIT_FOREVER);
        if(signal & LOCATION_TRACKING_FIXED_BIT)
        {
            atel_dbg_print("LAT: %d.%d LON: %d.%d ALT: %d.%d ACC: %d.%d",
            (int)location.latitude, ((int)(location.latitude*100000))%100000,
            (int)location.longitude, ((int)(location.longitude*100000))%100000,
            (int)location.altitude, ((int)(location.altitude*100))%100,
            (int)location.accuracy, ((int)(location.accuracy*100))%100);

            atel_dbg_print("SPEED: %d.%d BEAR: %d.%d TIME: %llu FLAGS: %x",
            (int)location.speed, ((int)(location.speed*100))%100,
            (int)location.bearing, ((int)(location.bearing*100))%100,location.timestamp,
            location.flags);    
            count++;
        }

		#if 0 // never exit gps service 
        if(count > 10)
        {
             atel_dbg_print("Stop gps sucsess");
             location_deinit();
             break;
        }
		#endif

    }
	return 0;
}

#endif /*__ATEL_BG96_APP__*/

