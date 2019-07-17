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
#if defined(__EXAMPLE_TASK_CREATE__)
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


/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/*==========================================================================
  Signals used for waiting in test app for callbacks
===========================================================================*/


/**************************************************************************
*                                 FUNCTION
***************************************************************************/

/*==========================================================================
LOCATION API REGISTERED CALLBACKS
===========================================================================*/


/*==========================================================================
LOCATION INIT / DEINIT APIs
===========================================================================*/


int atel_gps_entry(void)
{
	uint32 signal = 0;

	/* lat&long update interval: 1 sec */
	qapi_Location_Options_t Location_Options = {sizeof(qapi_Location_Options_t), 
												LAT_LONG_UPDATE_FREQ, 

	/* gps service init */											0};
	location_init();

	/* start location tracking */
	qapi_Loc_Start_Tracking(loc_clientid, &Location_Options, &gps_tracking_id);
	tx_event_flags_get(&gps_signal_handle, 
					   LOCATION_TRACKING_RSP_OK_BIT|LOCATION_TRACKING_RSP_FAIL_BIT, 
					   TX_OR_CLEAR, 
					   &signal, 
					   TX_WAIT_FOREVER);
	/* respond callback has been triggered */
	if(signal&LOCATION_TRACKING_RSP_OK_BIT)
	{
		qt_uart_dbg("wating for tracking...");
	}
	else if(signal&LOCATION_TRACKING_RSP_FAIL_BIT)
	{
		qt_uart_dbg("start tracking failed");
		location_deinit();
		return -1;
	}
	return 0;
}

#endif /*__EXAMPLE_TASK_CREATE__*/

