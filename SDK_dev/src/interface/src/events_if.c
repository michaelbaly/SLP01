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
#include "events_if.h"





/**************************************************************************
*                                 DEFINE
***************************************************************************/

/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/
/*
@func
  events_report
@brief
  report events based on event type
*/
void events_report(EVENTS_T type, int report_flag)
{
	return;
}

/*
@func
  alarms_report
@brief
  report events based on event type
*/
void alarms_report(EVENTS_T type, int report_flag)
{
	return;
}


void alarm_event_process(void)
{
	/* check alarm first */

    /* check event */

	return;
}


/*
@func
  cmd_parse
@brief
  receive cmd from tcp/udp server
*/
void cmd_parse(char *cmd_str)
{
	/* extract each para from cmd_str */
	
	return;
}



#endif /*__EXAMPLE_BG96_APP__*/



