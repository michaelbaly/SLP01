/******************************************************************************
*@file    example_task.h
*@brief   example of new task creation
*
*  ---------------------------------------------------------------------------
*
*  Copyright (c) 2018 Quectel Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Quectel Technologies, Inc.
*  ---------------------------------------------------------------------------
*******************************************************************************/
#ifndef __ATEL_BG96_APP_H__
#define __ATEL_BG96_APP_H__

#if defined(__ATEL_BG96_APP__)
#include "txm_module.h"

#include "qapi_fs_types.h"
#include "qapi_location.h"
#include "quectel_gpio.h"
#include "qapi_fs.h"
#include "qapi_socket.h"
#include "events_if.h"

#include <locale.h>

#define HIGH  1
#define LOW   0
#define TRUE  1
#define FALSE 0

#define EVENTS_SIG_ALL 	0x3ffffffff /* |bit31|bit30|bit29|...|bit0| 
										0	  0     1    ...  1 */

/* begin: for subtask of tcp client */
#define ATEL_TCPCLIENT_THREAD_PRIORITY   	180
#define ATEL_TCPCLIENT_THREAD_STACK_SIZE 	(1024 * 32)

//static TX_THREAD* atel_tcpclient_thread_handle;
static unsigned char atel_tcpclient_thread_stack[ATEL_TCPCLIENT_THREAD_STACK_SIZE];
/* end */

/* begin: for subtask of udp client */
#define ATEL_UDPCLIENT_THREAD_PRIORITY   	100
#define ATEL_UDPCLIENT_THREAD_STACK_SIZE 	(1024 * 32)

//static TX_THREAD* atel_tcpclient_thread_handle;
static unsigned char atel_udpclient_thread_stack[ATEL_UDPCLIENT_THREAD_STACK_SIZE];
/* end */


/* begin: for subtask of gps service */
#define ATEL_GPS_THREAD_PRIORITY   	180
#define ATEL_GPS_THREAD_STACK_SIZE 	(1024 * 32)

//static TX_THREAD* atel_gps_thread_handle;
static unsigned char atel_gps_thread_stack[ATEL_GPS_THREAD_STACK_SIZE];
/* end */

/* begin: for subtask of event report service */
#define ATEL_EVENT_REP_THREAD_PRIORITY   	180
#define ATEL_EVENT_REP_THREAD_STACK_SIZE 	(1024 * 32)

//static TX_THREAD* atel_event_rep_thread_handle;
static unsigned char atel_event_rep_thread_stack[ATEL_EVENT_REP_THREAD_STACK_SIZE];
/* end */


/* begin: for subtask of mdm communicate with ble through at framework */
#define ATEL_AT_FRAME_THREAD_PRIORITY   	180
#define ATEL_AT_FRAME_THREAD_STACK_SIZE 	(1024 * 32)

static TX_THREAD* atel_at_frame_thread_handle;
static unsigned char atel_at_frame_thread_stack[ATEL_AT_FRAME_THREAD_STACK_SIZE];
/* end */


/* struct for subtask */
typedef struct  module_task_config{
	
    TX_THREAD* module_thread_handle;
	const char* module_task_name;
	int (*module_task_entry)(void);
	unsigned char* module_thread_stack;
	unsigned int stack_size;
	unsigned int stack_prior;
	
}subtask_config_t;


typedef enum {
	IG_ON_WATCH_M = 0,
	IG_ON_THEFT_M,
	IG_ON_INVALID_M
}IG_ON_RUNNING_MODE_E;

#define IG_ON_DEFAULT_M IG_ON_WATCH_M

/* begin: ig status switch event*/
typedef  unsigned char      boolean; 
#define SWITCH_ON	1
#define SWITCH_OFF	0
#define NO_SWITCH	-1

#define SHIFT_L(n) (1UL<<n)
typedef enum {
	IG_SWITCH_ON_E = SHIFT_L(0),
	IG_SWITCH_OFF_E = SHIFT_L(1),
}IG_SWITCH_STATUS_E;
/* end */


#define QUEUE_NODE_MAX		40

typedef struct event_indicate_s{
	int prio;
	int tmp_data_addr;
}event_msg;



void loc_info_transform(qapi_Location_t location, char* trans_buf);
int ota_service_start(void);

extern	int atel_tcpclient_entry(void);
extern	int atel_udpclient_entry(void);
extern  int atel_gps_entry(void);
extern  int atel_mdm_ble_entry(void);
extern  int atel_led_on(MODULE_PIN_ENUM m_pin);
extern  int atel_led_off(MODULE_PIN_ENUM m_pin);
extern  int mm16_lan_power_on(MODULE_PIN_ENUM m_pin);

/* from events if */
extern void alarm_event_process(void);



#endif /*__ATEL_BG96_APP__*/
#endif /*__ATEL_BG96_APP_H__*/

