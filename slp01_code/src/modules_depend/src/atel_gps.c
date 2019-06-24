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
static void location_capabilities_callback(qapi_Location_Capabilities_Mask_t capabilities)
{

}

static void location_response_callback(qapi_Location_Error_t err, uint32_t id)
{

}

static void location_geofence_response_callback(size_t count,
                                                qapi_Location_Error_t* err,
                                                uint32_t* ids)
{
}

static void location_tracking_callback(qapi_Location_t loc)
{
}

qapi_Location_Callbacks_t location_callbacks= {
    sizeof(qapi_Location_Callbacks_t),
    location_capabilities_callback,
    location_response_callback,
    location_geofence_response_callback,
    location_tracking_callback,
    NULL,
    NULL
};

/*==========================================================================
LOCATION INIT / DEINIT APIs
===========================================================================*/
void location_init(void)
{
}

void location_deinit(void)
{
}

int quectel_gps_task_entry(void)
{

	return 0;
}

#endif /*__EXAMPLE_TASK_CREATE__*/

