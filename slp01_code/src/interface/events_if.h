/******************************************************************************
*@file    events_if.c
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


typedef enum {
	EVENT_E=0,
	ALARM_E,
	EVENT_IF_MAX
}EVENTS_T;


typedef enum {
	IGNITION_ON_E=0,
	IGNITION_OFF_E,
	TOWED,
	HARD_ACC,
	HARD_BRAKE,
	SHARP_TURN,
	QUICK_LANE_CHANGE,
	PLUG_INDICATE,
	UNPLUG_NOTIFY,
	MIL_ON,
	MIL_OFF,
	NORMAL_EVENTS_NR_MAX
	
}NORMAL_EVENTS_T;

typedef enum {
	ENGINE_TEM_HIGH=0,
	SPEEDING,
	BATTARY_LOW,
	ENGINE_IDLE_TIME,
	FATIGUE_DRIVE,
	VIBRATION,
	ALARM_NR_MAX
	
}ALARM_EVENTS_T;
/**************************************************************************
*                                 DEFINE
***************************************************************************/

/**************************************************************************
*                                 GLOBAL
***************************************************************************/

/**************************************************************************
*                           FUNCTION DECLARATION
***************************************************************************/




#endif /*__EXAMPLE_BG96_APP__*/




