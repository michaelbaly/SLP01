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
#include "qapi_fs_types.h"
#include "txm_module.h"

#include "qapi_fs_types.h"
#include "qapi_location.h"
#include "quectel_gpio.h"
#include "qapi_fs.h"
#include "qapi_socket.h"

#include <locale.h>

#define ATEL_TCPCLIENT_THREAD_PRIORITY   	180
#define ATEL_TCPCLIENT_THREAD_STACK_SIZE 	(1024 * 32)

static TX_THREAD* atel_tcpclient_thread_handle;
static unsigned char atel_tcpclient_thread_stack[ATEL_TCPCLIENT_THREAD_STACK_SIZE];

/* struct for tcp client subtask */
typedef struct  module_task_config{
	
    TX_THREAD* module_thread_handle;
	const char* module_task_name;
	int (*module_task_entry)(void);
	unsigned char* module_thread_stack;
	unsigned int stack_size;
	unsigned int stack_prior;
	
}subtask_config_t;

#define QT_Q_MAX_INFO_NUM		16

typedef struct TASK_COMM_S{
	int msg_id;
	int dat;
	CHAR name[16];
	CHAR buffer[32];
}TASK_MSG;

void loc_info_transform(qapi_Location_t location, char* trans_buf);


extern	int atel_tcpclient_start(void);
extern  int atel_led_on(MODULE_PIN_ENUM m_pin);
extern  int atel_led_off(MODULE_PIN_ENUM m_pin);
extern  int mm16_lan_power_on(MODULE_PIN_ENUM m_pin);


#endif /*__ATEL_BG96_APP__*/
#endif /*__ATEL_BG96_APP_H__*/

