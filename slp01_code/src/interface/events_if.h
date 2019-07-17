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

typedef struct bitsmap_alarm_event_s {
	/* common */
	unsigned int alarm_eve:1; /* bit0 battery low alarm */
	unsigned int alarm_eve:1; /* bit1 geofence break alarm */
	unsigned int alarm_eve:1; /* bit2 daily heartbeat event */
	
	/* for ig on situation */
	unsigned int alarm_eve:1; /* bit3 ignition on event */
	unsigned int alarm_eve:1; /* bit4 speeding */

    /* for ig off situation */
	unsigned int alarm_eve:1; /* bit5 ignition off event */
	unsigned int alarm_eve:1; /* bit6 towed */
	unsigned int alarm_eve:1; /* bit7 parking alert event */
	
	unsigned int alarm_eve:25; /* reserved */

	unsigned int drv_behav:1; /* bit0 hard acc */
	unsigned int drv_behav:1; /* bit1 hard brake */
	unsigned int drv_behav:1; /* bit2 hard brake */
	unsigned int drv_behav:1; /* bit3 sharp turn */
	unsigned int drv_behav:1; /* bit4 quick lane change */
	unsigned int drv_behav:1; /* bit5 speeding */
	unsigned int drv_behav:1; /* bit6 engine idle time */
	unsigned int drv_behav:1; /* bit7 fatigue drive */
	
	unsigned int drv_behav:25; /* reserved */
}AE_BITMAP_T;

#define BIT_SET(alarm_eve, n) 			(alarm_eve |= SHIFT_L(n))
#define BIT_CLEAR(alarm_eve, n)			(alarm_eve &= ~(SHIFT_L(n)))
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




